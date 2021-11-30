/**
 * Copyright 2021 Wolkabout s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/utilities/Logger.h"
#include "wolk/service/file_management/poco/PocoFileDownloader.h"

#include <Poco/Crypto/Crypto.h>
#include <Poco/Util/Util.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <regex>

namespace wolkabout
{
// Here we store prefixes used by the static methods.
const std::string HTTP_PATH_PREFIX = "http://";
const std::string HTTPS_PATH_PREFIX = "https://";
const std::regex URL_REGEX = std::regex(
  R"(https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*))");

PocoFileDownloader::PocoFileDownloader(CommandBuffer& commandBuffer)
: m_status(FileUploadStatus::AWAITING_DEVICE), m_commandBuffer(commandBuffer)
{
}

PocoFileDownloader::~PocoFileDownloader()
{
    LOG(TRACE) << METHOD_INFO;
    stop();
}

FileUploadStatus PocoFileDownloader::getStatus()
{
    return m_status;
}

std::string PocoFileDownloader::getName()
{
    return m_name;
}

ByteArray PocoFileDownloader::getBytes()
{
    return m_bytes;
}

void PocoFileDownloader::downloadFile(
  const std::string& url, std::function<void(FileUploadStatus, FileUploadError, std::string)> statusCallback)
{
    LOG(TRACE) << METHOD_INFO;

    // Place the information where it needs to be placed
    m_statusCallback = statusCallback;

    // Check the URL with the regex
    if (!std::regex_search(url, URL_REGEX))
    {
        changeStatus(FileUploadStatus::ERROR, FileUploadError::MALFORMED_URL, "");
        return;
    }

    // Start the thread that will do all the work
    m_thread = std::unique_ptr<std::thread>{new std::thread{&PocoFileDownloader::download, this, url}};
}

void PocoFileDownloader::abortDownload()
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the last sent status is not aborted, error, or ready
    if (m_session != nullptr &&
        (m_status == FileUploadStatus::AWAITING_DEVICE || m_status == FileUploadStatus::FILE_TRANSFER))
    {
        {
            // Change the status to ABORTED
            std::lock_guard<std::mutex> lock{m_mutex};
            m_status = FileUploadStatus::ABORTED;
            changeStatus(FileUploadStatus::ABORTED, FileUploadError::NONE, {});
        }

        // And stop everything that is running
        stop();
    }
}

void PocoFileDownloader::download(const std::string& url)
{
    LOG(TRACE) << METHOD_INFO;

    // Start by creating the session
    changeStatus(FileUploadStatus::FILE_TRANSFER, FileUploadError::NONE, {});
    auto host = extractHost(url);
    auto port = extractPort(url);
    // TODO Uncomment this!
    //    if (url.find("https") != std::string::npos)
    //        m_session = std::unique_ptr<Poco::Net::HTTPSClientSession>{new Poco::Net::HTTPSClientSession{host, port}};
    //    else
    m_session = std::unique_ptr<Poco::Net::HTTPClientSession>{new Poco::Net::HTTPClientSession{host, port}};

    // Create the request
    auto uri = extractUri(url);
    auto request = std::unique_ptr<Poco::Net::HTTPRequest>(new Poco::Net::HTTPRequest("GET", uri));
    m_session->sendRequest(*request);

    // Await the response
    auto response = std::unique_ptr<Poco::Net::HTTPResponse>(new Poco::Net::HTTPResponse);
    auto outputStream = std::stringstream{};
    Poco::StreamCopier::copyStream(m_session->receiveResponse(*response), outputStream);

    // Check the code of the response
    if (response->getStatus() != Poco::Net::HTTPResponse::HTTP_OK)
    {
        changeStatus(FileUploadStatus::ERROR, FileUploadError::MALFORMED_URL, "");
        return;
    }

    // Make it into a byte stream
    {
        auto output = outputStream.str();
        m_bytes.reserve(output.size());
        m_bytes.assign(output.cbegin(), output.cend());
    }
    outputStream.clear();

    // Decide on the name of the file
    auto name = uri.substr(uri.rfind('/'));
    if (name.find('?'))
        name = name.substr(0, name.find('?'));

    // Now with everything set, we can announce everything
    changeStatus(FileUploadStatus::FILE_READY, FileUploadError::NONE, name);
}

void PocoFileDownloader::stop()
{
    LOG(TRACE) << METHOD_INFO;

    // Stop the client session
    if (m_session != nullptr)
    {
        m_session->abort();
        m_session.reset();
    }

    // Join the thread
    if (m_thread != nullptr)
    {
        if (m_thread->joinable())
            m_thread->join();
        m_thread.reset();
    }
}

void PocoFileDownloader::changeStatus(FileUploadStatus status, FileUploadError error, const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Lock the mutex, change the local status
    std::lock_guard<std::mutex> lock{m_mutex};
    if (status != m_status)
    {
        // Change the status
        m_status = status;
        m_name = fileName;

        // Check if there's a callback to call
        if (m_statusCallback)
        {
            m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(
              [this, status, error, fileName]() { this->m_statusCallback(status, error, fileName); }));
        }
    }
}

std::string PocoFileDownloader::extractHost(std::string targetPath)
{
    // Manipulate the copy of the string to extract the host
    if (targetPath.find(HTTP_PATH_PREFIX) != std::string::npos)
        targetPath.replace(targetPath.find(HTTP_PATH_PREFIX), HTTP_PATH_PREFIX.length(), "");
    if (targetPath.find(HTTPS_PATH_PREFIX) != std::string::npos)
        targetPath.replace(targetPath.find(HTTPS_PATH_PREFIX), HTTPS_PATH_PREFIX.length(), "");
    if (targetPath.find(':') != std::string::npos)
        return targetPath.substr(0, targetPath.find(':'));
    if (targetPath.find('/') != std::string::npos)
        return targetPath.substr(0, targetPath.find('/'));
    return targetPath;
}

std::uint16_t PocoFileDownloader::extractPort(std::string targetPath)
{
    // Take out the HTTP(S) prefix
    if (targetPath.find(HTTP_PATH_PREFIX) != std::string::npos)
        targetPath.replace(targetPath.find(HTTP_PATH_PREFIX), HTTP_PATH_PREFIX.length(), "");
    if (targetPath.find(HTTPS_PATH_PREFIX) != std::string::npos)
        targetPath.replace(targetPath.find(HTTPS_PATH_PREFIX), HTTPS_PATH_PREFIX.length(), "");
    // Look for the port inside the string
    if (targetPath.find(':') != std::string::npos)
    {
        auto substring = targetPath.substr(targetPath.find(':') + 1, targetPath.find('/'));
        return static_cast<uint16_t>(std::stoul(substring));
    }
    return 80;
}

std::string PocoFileDownloader::extractUri(std::string targetPath)
{
    // Take out the HTTP(S) prefix
    if (targetPath.find(HTTP_PATH_PREFIX) != std::string::npos)
        targetPath.replace(targetPath.find(HTTP_PATH_PREFIX), HTTP_PATH_PREFIX.length(), "");
    if (targetPath.find(HTTPS_PATH_PREFIX) != std::string::npos)
        targetPath.replace(targetPath.find(HTTPS_PATH_PREFIX), HTTPS_PATH_PREFIX.length(), "");

    // Everything with the and after the first slash is a uri now
    if (targetPath.find('/') != std::string::npos)
        return targetPath.substr(targetPath.find('/'));
    return "/";
}
}    // namespace wolkabout

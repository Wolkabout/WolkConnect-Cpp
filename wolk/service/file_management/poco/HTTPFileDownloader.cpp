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

#include "wolk/service/file_management/poco/HTTPFileDownloader.h"

#include "core/utilities/Logger.h"

#include <Poco/Net/Context.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SecureStreamSocket.h>
#include <Poco/StreamCopier.h>
#include <Poco/Util/Util.h>
#include <iomanip>
#include <regex>

namespace wolkabout
{
// Here we store prefixes used by the static methods.
const std::string HTTP_PATH_PREFIX = "http://";
const std::string HTTPS_PATH_PREFIX = "https://";
const std::regex URL_REGEX = std::regex(
  R"(https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*))");

HTTPFileDownloader::HTTPFileDownloader(CommandBuffer& commandBuffer)
: m_status(FileUploadStatus::AWAITING_DEVICE), m_commandBuffer(commandBuffer)
{
}

HTTPFileDownloader::~HTTPFileDownloader()
{
    LOG(TRACE) << METHOD_INFO;
    stop();
}

FileUploadStatus HTTPFileDownloader::getStatus()
{
    return m_status;
}

std::string HTTPFileDownloader::getName()
{
    return m_name;
}

ByteArray HTTPFileDownloader::getBytes()
{
    return m_bytes;
}

void HTTPFileDownloader::downloadFile(
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
    if (m_thread != nullptr && m_thread->joinable())
        m_thread->join();
    m_thread = std::unique_ptr<std::thread>{new std::thread{&HTTPFileDownloader::download, this, url}};
}

void HTTPFileDownloader::abortDownload()
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

void HTTPFileDownloader::download(const std::string& url)
{
    LOG(TRACE) << METHOD_INFO;

    try
    {
        // Start by creating the session
        changeStatus(FileUploadStatus::FILE_TRANSFER, FileUploadError::NONE, {});
        auto host = extractHost(url);
        auto port = extractPort(url);

        if (url.find(HTTPS_PATH_PREFIX) != std::string::npos)
        {
            // Create the SSL session with the context
            Poco::Net::Context::Ptr context = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "",
                                                                     Poco::Net::Context::VerificationMode::VERIFY_NONE);
            context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
            m_session =
              std::unique_ptr<Poco::Net::HTTPSClientSession>{new Poco::Net::HTTPSClientSession{host, 443, context}};
        }
        else
        {
            // Create just a plain old HTTP session
            m_session = std::unique_ptr<Poco::Net::HTTPClientSession>{new Poco::Net::HTTPClientSession{host, port}};
        }

        // Create the request
        auto uri = extractUri(url);
        auto request = Poco::Net::HTTPRequest("GET", uri, Poco::Net::HTTPRequest::HTTP_1_1);
        m_session->sendRequest(request) << "";

        // Await the response
        Poco::Net::HTTPResponse response;
        auto& body = m_session->receiveResponse(response);

        // Check the code of the response
        if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK)
        {
            changeStatus(FileUploadStatus::ERROR, FileUploadError::MALFORMED_URL, "");
            return;
        }

        // Copy the response in to the bytes
        auto contentStream = std::stringstream{};
        Poco::StreamCopier::copyStream(body, contentStream);
        auto content = contentStream.str();
        m_bytes = ByteUtils::toByteArray(content);

        // Decide on the name of the file
        auto name = uri.substr(uri.rfind('/') + 1);
        if (name.find('?'))
            name = name.substr(0, name.find('?'));
        if (name.empty())
        {
            // Get the SHA256 of the file and name it that
            auto hash = ByteUtils::hashSHA256(m_bytes);
            auto hashStringStream = std::stringstream{};
            for (const auto& hashByte : hash)
                hashStringStream << std::setfill('0') << std::setw(2) << std::hex
                                 << static_cast<std::int32_t>(hashByte);
            name = hashStringStream.str();
        }

        // Now with everything set, we can announce everything
        changeStatus(FileUploadStatus::FILE_READY, FileUploadError::NONE, name);
    }
    catch (const Poco::Exception& exception)
    {
        LOG(ERROR) << "An error has occurred while downloading the file -> '" << exception.message() << "'.";
        changeStatus(FileUploadStatus::ERROR, FileUploadError::MALFORMED_URL, {});
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "An error has occurred while downloading the file -> '" << exception.what() << "'.";
        changeStatus(FileUploadStatus::ERROR, FileUploadError::MALFORMED_URL, {});
    }
}

void HTTPFileDownloader::stop()
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

void HTTPFileDownloader::changeStatus(FileUploadStatus status, FileUploadError error, const std::string& fileName)
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

std::string HTTPFileDownloader::extractHost(std::string targetPath)
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

std::uint16_t HTTPFileDownloader::extractPort(std::string targetPath)
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

std::string HTTPFileDownloader::extractUri(std::string targetPath)
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

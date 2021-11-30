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

#include "core/model/messages/FileBinaryRequestMessage.h"
#include "core/model/messages/FileBinaryResponseMessage.h"
#include "core/model/messages/FileUploadInitiateMessage.h"
#include "core/model/messages/FileUrlDownloadInitMessage.h"
#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"
#include "wolk/service/file_management/FileTransferSession.h"

#include <iomanip>
#include <openssl/md5.h>
#include <utility>

namespace wolkabout
{
FileTransferSession::FileTransferSession(const FileUploadInitiateMessage& message,
                                         std::function<void(FileUploadStatus, FileUploadError)> callback,
                                         CommandBuffer& commandBuffer)
: m_name(message.getName())
, m_retryCount(0)
, m_done(false)
, m_size(message.getSize())
, m_hash(message.getHash())
, m_status(FileUploadStatus::FILE_TRANSFER)
, m_error(FileUploadError::NONE)
, m_callback(std::move(callback))
, m_commandBuffer(commandBuffer)
{
}

FileTransferSession::FileTransferSession(const FileUrlDownloadInitMessage& message,
                                         std::function<void(FileUploadStatus, FileUploadError)> callback,
                                         CommandBuffer& commandBuffer, std::shared_ptr<FileDownloader> fileDownloader)
: m_url(message.getPath())
, m_retryCount(0)
, m_done(false)
, m_size(0)
, m_downloader(std::move(fileDownloader))
, m_status(FileUploadStatus::FILE_TRANSFER)
, m_error(FileUploadError::NONE)
, m_callback(std::move(callback))
, m_commandBuffer(commandBuffer)
{
}

bool FileTransferSession::isPlatformTransfer() const
{
    return m_url.empty();
}

bool FileTransferSession::isUrlDownload() const
{
    return !isPlatformTransfer();
}

bool FileTransferSession::isDone() const
{
    return m_done;
}

const std::string& FileTransferSession::getName() const
{
    return m_name;
}

const std::string& FileTransferSession::getUrl() const
{
    return m_url;
}

void FileTransferSession::abort()
{
    LOG(TRACE) << METHOD_INFO;

    // Based on the type of transfer
    if (isPlatformTransfer())
        // Clean up the chunks
        m_chunks.clear();
    else
        // Tell the downloader to abort
        m_downloader->abortDownload();

    // Report the statuses properly
    m_done = true;
    changeStatusAndError(FileUploadStatus::ABORTED, FileUploadError::NONE);
}

FileUploadError FileTransferSession::pushChunk(const FileBinaryResponseMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the transfer is an upload session
    if (isUrlDownload())
    {
        LOG(DEBUG) << "Failed to receive FileBinaryResponseMessage -> The transfer session is not a platform transfer.";
        return FileUploadError::TRANSFER_PROTOCOL_DISABLED;
    }

    // Check if there is a need for this chunk even
    auto collectedSize = std::uint64_t{0};
    for (const auto& chunk : m_chunks)
        collectedSize += chunk.bytes.size();
    if (collectedSize >= m_size)
    {
        LOG(DEBUG) << "Failed to receive FileBinaryResponseMessage -> The session has already collected enough bytes "
                      "for this session.";
        return FileUploadError::UNSUPPORTED_FILE_SIZE;
    }

    // Check the hash with the previous chunk (if it exists)
    if (!m_chunks.empty())
    {
        // Take the last chunk
        const auto& lastChunk = m_chunks[m_chunks.size() - 1];
        if (lastChunk.hash != message.getPreviousHash())
        {
            LOG(DEBUG) << "Failed to receive FileBinaryResponseMessage -> The previous hash of the current message and "
                          "hash of the previous chunk do not match.";
            if (m_retryCount++ >= 3)
            {
                m_done = true;
                changeStatusAndError(FileUploadStatus::ERROR, FileUploadError::RETRY_COUNT_EXCEEDED);
                return FileUploadError::RETRY_COUNT_EXCEEDED;
            }
            return FileUploadError::FILE_HASH_MISMATCH;
        }
    }

    // If the currents chunk data hash value is not valid, also we got to report that
    auto sendHash = ByteUtils::toByteArray(message.getCurrentHash());
    auto currentHash = ByteUtils::hashSHA256(message.getData());
    for (auto i = std::uint32_t{0}; i < sendHash.size(); ++i)
    {
        if (sendHash[i] != currentHash[i])
        {
            LOG(DEBUG) << "Failed to receive FileBinaryResponseMessage -> The previous hash of the current message and "
                          "hash of the previous chunk do not match.";
            if (m_retryCount++ >= 3)
            {
                m_done = true;
                changeStatusAndError(FileUploadStatus::ERROR, FileUploadError::RETRY_COUNT_EXCEEDED);
                return FileUploadError::RETRY_COUNT_EXCEEDED;
            }
            return FileUploadError::FILE_HASH_MISMATCH;
        }
    }

    // Push the chunk in the vector
    m_chunks.emplace_back(FileChunk{message.getPreviousHash(), message.getData(), message.getCurrentHash()});
    collectedSize += message.getData().size();

    // Check if the size is now the file size
    if (collectedSize >= m_size)
    {
        LOG(DEBUG) << "Collected all the bytes in FileTransferSession of file '" << m_name << "'.";
        m_done = true;

        // Hash the value
        std::uint8_t hashCStr[MD5_DIGEST_LENGTH];
        auto context = MD5_CTX();
        MD5_Init(&context);

        // Now we need to check the hash
        for (const auto& chunk : m_chunks)
            MD5_Update(&context, chunk.bytes.data(), chunk.bytes.size());
        MD5_Final(hashCStr, &context);

        // Put that into a string stream
        auto hashStream = std::stringstream{};
        for (auto i = std::uint32_t{0}; i < MD5_DIGEST_LENGTH; ++i)
            hashStream << std::setfill('0') << std::setw(2) << std::hex << static_cast<std::int32_t>(hashCStr[i]);
        auto hash = hashStream.str();

        // Now check the hash
        if (hash == m_hash)
            changeStatusAndError(FileUploadStatus::FILE_READY, FileUploadError::NONE);
        else
            changeStatusAndError(FileUploadStatus::ERROR, FileUploadError::FILE_HASH_MISMATCH);
    }
    return FileUploadError::NONE;
}

FileBinaryRequestMessage FileTransferSession::getNextChunkRequest()
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the transfer is an upload session
    if (isUrlDownload())
    {
        LOG(DEBUG) << "Failed to return FileBinaryRequestMessage -> The transfer session is not a platform transfer.";
        return FileBinaryRequestMessage{"", 0};
    }

    // Check if there are bytes still missing, and if there are, just take the size of chunk vector, and return the
    // size.
    auto collectedSize = std::uint64_t{0};
    for (const auto& chunk : m_chunks)
        collectedSize += chunk.bytes.size();
    if (collectedSize >= m_size)
    {
        LOG(DEBUG)
          << "Failed to return FileBinaryRequestMessage -> The session has obtained enough bytes for this file.";
        return FileBinaryRequestMessage{"", 0};
    }
    else
    {
        LOG(DEBUG) << "Successfully returned next FileBinaryRequestMessage for chunk " << m_chunks.size() << ".";
        return FileBinaryRequestMessage{m_name, m_chunks.size()};
    }
}

bool FileTransferSession::triggerDownload()
{
    LOG(TRACE) << METHOD_INFO;

    // Check that the session is related to URL download
    if (isPlatformTransfer())
    {
        LOG(DEBUG) << "Failed to trigger a URL download - The session is not a URL download session.";
        return false;
    }

    // Check if we have a downloader.
    if (m_downloader == nullptr)
    {
        LOG(DEBUG) << "Failed to trigger a URL download - We are missing a FileDownloader.";
        return false;
    }

    // Now that we have adequate information, setup everything
    m_downloader->downloadFile(m_url,
                               [this](FileUploadStatus status, FileUploadError error, const std::string& fileName)
                               {
                                   // Set the name if a name value is sent out
                                   if (!fileName.empty())
                                       this->m_name = fileName;

                                   // Announce the status
                                   changeStatusAndError(status, error);
                               });
    return true;
}

FileUploadStatus FileTransferSession::getStatus() const
{
    return m_status;
}

FileUploadError FileTransferSession::getError() const
{
    return m_error;
}

const std::vector<FileChunk>& FileTransferSession::getChunks() const
{
    return m_chunks;
}

void FileTransferSession::changeStatusAndError(FileUploadStatus status, FileUploadError error)
{
    LOG(TRACE) << METHOD_INFO;

    // Lock the mutex for all this
    std::lock_guard<std::mutex> lock{m_mutex};

    // Check if the status value is changed
    auto statusChanged = status != m_status;
    auto errorChanged = error != m_error;

    // Go into changing the values if any of them changed
    if (statusChanged || errorChanged)
    {
        // Change the member values
        m_status = status;
        m_error = error;

        // Queue the callback call
        if (m_callback)
            m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(
              [this, status, error]()
              {
                  if (this->m_callback)
                      this->m_callback(status, error);
              }));
    }
}
}    // namespace wolkabout

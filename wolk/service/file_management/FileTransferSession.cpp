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
#include "core/utilities/Logger.h"
#include "wolk/service/file_management/FileTransferSession.h"

#include <utility>

namespace wolkabout
{
FileTransferSession::FileTransferSession(const FileUploadInitiateMessage& message,
                                         std::function<void(FileUploadStatus, FileUploadError)> callback,
                                         std::uint64_t maxSize)
: m_name(message.getName())
, m_maxSize(maxSize)
, m_size(message.getSize())
, m_hash(message.getHash())
, m_status(FileUploadStatus::AWAITING_DEVICE)
, m_error(FileUploadError::NONE)
, m_callback(std::move(callback))
{
}

FileTransferSession::FileTransferSession(const FileUrlDownloadInitMessage& message,
                                         std::function<void(FileUploadStatus, FileUploadError)> callback)
: m_url(message.getPath())
, m_maxSize(0)
, m_size(0)
, m_status(FileUploadStatus::AWAITING_DEVICE)
, m_error(FileUploadError::NONE)
, m_callback(std::move(callback))
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

const std::string& FileTransferSession::getName() const
{
    return m_name;
}

const std::string& FileTransferSession::getUrl() const
{
    return m_url;
}

FileUploadStatus FileTransferSession::pushChunk(const FileBinaryResponseMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
    return FileUploadStatus::FILE_TRANSFER;
}

FileBinaryRequestMessage FileTransferSession::getNextChunkRequest()
{
    LOG(TRACE) << METHOD_INFO;
    return FileBinaryRequestMessage{"", 0};
}

FileUploadStatus FileTransferSession::getStatus() const
{
    return m_status;
}

FileUploadError FileTransferSession::getError() const
{
    return m_error;
}
}    // namespace wolkabout

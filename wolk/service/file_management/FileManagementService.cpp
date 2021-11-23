/**
 * Copyright 2021 WolkAbout Technology s.r.o.
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

#include "core/model/Message.h"
#include "core/utilities/Logger.h"
#include "wolk/service/file_management/FileManagementService.h"

#include <utility>

namespace wolkabout
{
FileManagementService::FileManagementService(ConnectivityService& connectivityService, FileManagementProtocol& protocol,
                                             std::string fileLocation, std::uint64_t maxPacketSize)
: m_connectivityService(connectivityService)
, m_protocol(protocol)
, m_fileLocation(std::move(fileLocation))
, m_maxPacketSize(maxPacketSize)
{
}

const Protocol& FileManagementService::getProtocol()
{
    return m_protocol;
}

void FileManagementService::messageReceived(std::shared_ptr<Message> message)
{
    // Check that the message is not null
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to process received message -> The message is null!";
        return;
    }

    // Look for the device this message is targeting
    auto type = m_protocol.getMessageType(message);
    auto target = m_protocol.extractDeviceKeyFromChannel(message->getChannel());
    LOG(TRACE) << "Received message '" << toString(type) << "' for target '" << target << "'.";

    // Parse the received message based on the type
    switch (type)
    {
    case MessageType::FILE_UPLOAD_INIT:
    {
        if (auto parsedMessage = m_protocol.parseFileUploadInit(message))
            onFileUploadInit(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse incoming 'FileUploadInitiate' message.";
        return;
    }
    case MessageType::FILE_UPLOAD_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFileUploadAbort(message))
            onFileUploadAbort(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FileUploadAbort' message.";
        return;
    }
    case MessageType::FILE_BINARY_RESPONSE:
    {
        if (auto parsedMessage = m_protocol.parseFileBinaryResponse(message))
            onFileBinaryResponse(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FileBinaryResponse' message.";
        return;
    }
    case MessageType::FILE_URL_DOWNLOAD_INIT:
    {
        if (auto parsedMessage = m_protocol.parseFileUrlDownloadInit(message))
            onFileUrlDownloadInit(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FileUrlDownloadInit' message.";
        return;
    }
    case MessageType::FILE_URL_DOWNLOAD_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFileUrlDownloadAbort(message))
            onFileUrlDownloadAbort(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FileUrlDownloadAbort' message.";
        return;
    }
    case MessageType::FILE_LIST_REQUEST:
    {
        if (auto parsedMessage = m_protocol.parseFileListRequest(message))
            onFileListRequest(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FileListRequest' message.";
        return;
    }
    case MessageType::FILE_DELETE:
    {
        if (auto parsedMessage = m_protocol.parseFileDelete(message))
            onFileDelete(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FileDelete' message.";
        return;
    }
    case MessageType::FILE_PURGE:
    {
        if (auto parsedMessage = m_protocol.parseFilePurge(message))
            onFilePurge(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FilePurge' message.";
        return;
    }
    default:
        LOG(ERROR) << "Received a message of invalid type for this service.";
        return;
    }
}

void FileManagementService::onFileUploadInit(const std::string& deviceKey, const FileUploadInitiateMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // TODO For now a test in development
    // Make a response status that will refuse this
    auto response = std::make_shared<FileUploadStatusMessage>(message.getName(), FileUploadStatus::ERROR,
                                                              FileUploadError::TRANSFER_PROTOCOL_DISABLED);
    m_connectivityService.publish(m_protocol.makeOutboundMessage(deviceKey, *response), false);
}

void FileManagementService::onFileUploadAbort(const std::string& deviceKey, const FileUploadAbortMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
}

void FileManagementService::onFileBinaryResponse(const std::string& deviceKey, const FileBinaryResponseMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
}

void FileManagementService::onFileUrlDownloadInit(const std::string& deviceKey,
                                                  const FileUrlDownloadInitMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
}

void FileManagementService::onFileUrlDownloadAbort(const std::string& deviceKey,
                                                   const FileUrlDownloadAbortMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
}

void FileManagementService::onFileListRequest(const std::string& deviceKey, const FileListRequestMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
    auto response = FileListResponseMessage({});
    m_connectivityService.publish(m_protocol.makeOutboundMessage(deviceKey, response), false);
}

void FileManagementService::onFileDelete(const std::string& deviceKey, const FileDeleteMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
}

void FileManagementService::onFilePurge(const std::string& deviceKey, const FilePurgeMessage& message)
{
    LOG(TRACE) << METHOD_INFO;
}
}    // namespace wolkabout

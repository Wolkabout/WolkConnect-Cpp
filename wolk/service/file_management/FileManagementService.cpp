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

#include "wolk/service/file_management/FileManagementService.h"

#include "core/model/Message.h"
#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"

#include <algorithm>
#include <iomanip>
#include <utility>

namespace wolkabout
{
FileManagementService::FileManagementService(ConnectivityService& connectivityService, DataService& dataService,
                                             FileManagementProtocol& protocol, std::string fileLocation,
                                             bool fileTransferEnabled, bool fileTransferUrlEnabled,
                                             std::shared_ptr<FileDownloader> fileDownloader,
                                             std::shared_ptr<FileListener> fileListener)
: m_connectivityService(connectivityService)
, m_dataService(dataService)
, m_fileTransferEnabled(fileTransferEnabled)
, m_fileTransferUrlEnabled(fileTransferUrlEnabled)
, m_protocol(protocol)
, m_fileLocation(std::move(fileLocation))
, m_downloader(std::move(fileDownloader))
, m_fileListener(std::move(fileListener))
{
    if (!(fileTransferEnabled || fileTransferUrlEnabled))
        throw std::runtime_error("Failed to create 'FileManagementService' with both flags disabled.");
}

void FileManagementService::createFolder()
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the folder by chance does not exist
    if (!FileSystemUtils::isDirectoryPresent(m_fileLocation))
    {
        FileSystemUtils::createDirectory(m_fileLocation);
        LOG(DEBUG) << "Created FileManagement directory '" << m_fileLocation << "'.";
    }
}

void FileManagementService::reportPresentFiles(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Make place for all the file information
    auto fileInformationVector = std::vector<FileInformation>{};

    // Find the folder for the device
    auto deviceFolder = FileSystemUtils::composePath(deviceKey, m_fileLocation);
    if (FileSystemUtils::isDirectoryPresent(deviceFolder))
    {
        // We can read the files
        auto folderContent = FileSystemUtils::listFiles(deviceFolder);

        // Check if the map contains an entry for this device, if not, make one
        if (m_files.find(deviceKey) == m_files.cend())
            m_files.emplace(deviceKey, DeviceFiles{});
        // Take the reference, because we need to check if we need to delete any local file info
        auto& fileRegistry = m_files[deviceKey];

        // First we need to check if we should delete anything from the local registry
        for (const auto& fileInRegistry : fileRegistry)
        {
            // Check if the file can be found in the folder
            const auto it = std::find(folderContent.cbegin(), folderContent.cend(), fileInRegistry.first);
            if (it == folderContent.cend())
                fileRegistry.erase(fileInRegistry.first);
        }

        // Form the information about all files the device holds
        for (const auto& file : folderContent)
        {
            // Obtain information about the file
            auto informationIt = fileRegistry.find(file);
            if (informationIt == fileRegistry.cend())
            {
                // Look for it now
                auto freshInformation = obtainFileInformation(deviceKey, file);
                if (freshInformation.name.empty())
                {
                    LOG(WARN) << "Failed to obtain FileInformation for file '" << file << "'.";
                    continue;
                }

                // Emplace it in the map
                informationIt = fileRegistry.emplace(file, freshInformation).first;
                LOG(DEBUG) << "Obtained local FileInformation for file '" << file << "'.";
                notifyListenerAddedFile(deviceKey, file, absolutePathOfFile(deviceKey, file));
            }

            // Emplace the complete information in the vector
            fileInformationVector.emplace_back(
              FileInformation{file, informationIt->second.size, informationIt->second.hash});
        }
    }

    // Make the message
    auto fileList = FileListResponseMessage{fileInformationVector};
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, fileList));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to obtain serialized 'FileList' message.";
        return;
    }
    m_connectivityService.publish(message);
}

const Protocol& FileManagementService::getProtocol()
{
    return m_protocol;
}

void FileManagementService::messageReceived(std::shared_ptr<Message> message)
{
    LOG(TRACE) << METHOD_INFO;

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
        {
            if (m_fileTransferEnabled)
            {
                onFileUploadInit(target, *parsedMessage);
            }
            else
            {
                reportTransferProtocolDisabled(target, parsedMessage->getName());
            }
        }
        else
        {
            LOG(ERROR) << "Failed to parse incoming 'FileUploadInitiate' message.";
        }
        return;
    }
    case MessageType::FILE_UPLOAD_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFileUploadAbort(message))
        {
            if (m_fileTransferEnabled)
            {
                onFileUploadAbort(target, *parsedMessage);
            }
            else
            {
                reportTransferProtocolDisabled(target, parsedMessage->getName());
            }
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FileUploadAbort' message.";
        }
        return;
    }
    case MessageType::FILE_BINARY_RESPONSE:
    {
        if (auto parsedMessage = m_protocol.parseFileBinaryResponse(message))
        {
            if (m_fileTransferEnabled)
            {
                onFileBinaryResponse(target, *parsedMessage);
            }
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FileBinaryResponse' message.";
        }
        return;
    }
    case MessageType::FILE_URL_DOWNLOAD_INIT:
    {
        if (auto parsedMessage = m_protocol.parseFileUrlDownloadInit(message))
        {
            if (m_fileTransferUrlEnabled)
            {
                onFileUrlDownloadInit(target, *parsedMessage);
            }
            else
            {
                reportUrlTransferProtocolDisabled(target, parsedMessage->getPath());
            }
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FileUrlDownloadInit' message.";
        }
        return;
    }
    case MessageType::FILE_URL_DOWNLOAD_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFileUrlDownloadAbort(message))
        {
            if (m_fileTransferUrlEnabled)
            {
                onFileUrlDownloadAbort(target, *parsedMessage);
            }
            else
            {
                reportUrlTransferProtocolDisabled(target, parsedMessage->getPath());
            }
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FileUrlDownloadAbort' message.";
        }
        return;
    }
    case MessageType::FILE_LIST_REQUEST:
    {
        if (auto parsedMessage = m_protocol.parseFileListRequest(message))
        {
            onFileListRequest(target, *parsedMessage);
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FileListRequest' message.";
        }
        return;
    }
    case MessageType::FILE_DELETE:
    {
        if (auto parsedMessage = m_protocol.parseFileDelete(message))
        {
            onFileDelete(target, *parsedMessage);
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FileDelete' message.";
        }
        return;
    }
    case MessageType::FILE_PURGE:
    {
        if (auto parsedMessage = m_protocol.parseFilePurge(message))
        {
            onFilePurge(target, *parsedMessage);
        }
        else
        {
            LOG(ERROR) << "Failed to parse 'FilePurge' message.";
        }
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

    // Check whether there is a session already ongoing
    if (m_sessions.find(deviceKey) != m_sessions.cend() && m_sessions[deviceKey] != nullptr)
    {
        LOG(DEBUG) << "Received a FileUploadInitiate message while a session is already ongoing. Ignoring...";
        return;
    }

    // Create a session for this file
    m_sessions.emplace(deviceKey, std::unique_ptr<FileTransferSession>(new FileTransferSession(
                                    deviceKey, message,
                                    [this, deviceKey](FileUploadStatus status, FileUploadError error)
                                    { this->onFileSessionStatus(deviceKey, status, error); },
                                    m_commandBuffer)));

    // Obtain the first message for the session
    auto firstMessage = m_sessions[deviceKey]->getNextChunkRequest();
    if (!firstMessage.getName().empty())
    {
        // Send out the status and the request
        reportStatus(deviceKey, FileUploadStatus::FILE_TRANSFER, FileUploadError::NONE);
        sendChunkRequest(deviceKey, firstMessage);
    }
}

void FileManagementService::onFileUploadAbort(const std::string& deviceKey, const FileUploadAbortMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    if (m_sessions.find(deviceKey) != m_sessions.cend() && m_sessions[deviceKey] != nullptr &&
        m_sessions[deviceKey]->getName() == message.getName())
        m_sessions[deviceKey]->abort();
}

void FileManagementService::onFileBinaryResponse(const std::string& deviceKey, const FileBinaryResponseMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // Pass the message onto the session
    if (m_sessions.find(deviceKey) != m_sessions.cend() && m_sessions[deviceKey] != nullptr &&
        m_sessions[deviceKey]->isPlatformTransfer())
    {
        // Pass the bytes onto it
        auto error = m_sessions[deviceKey]->pushChunk(message);
        if (error == FileUploadError::FILE_HASH_MISMATCH || !m_sessions[deviceKey]->isDone())
            sendChunkRequest(deviceKey, m_sessions[deviceKey]->getNextChunkRequest());
    }
}

void FileManagementService::onFileUrlDownloadInit(const std::string& deviceKey,
                                                  const FileUrlDownloadInitMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // We need to attempt to create a session.
    if (m_sessions.find(deviceKey) != m_sessions.cend() && m_sessions[deviceKey] != nullptr)
    {
        LOG(DEBUG) << "Received a FileUrlDownloadInit message while a session is already ongoing. Ignoring...";
        return;
    }

    // Create a session for this message
    m_sessions[deviceKey] = std::unique_ptr<FileTransferSession>(new FileTransferSession(
      deviceKey, message,
      [this, deviceKey](FileUploadStatus status, FileUploadError error)
      { this->onFileSessionStatus(deviceKey, status, error); },
      m_commandBuffer, m_downloader));

    // Trigger the download
    m_sessions[deviceKey]->triggerDownload();
    reportStatus(deviceKey, FileUploadStatus::FILE_TRANSFER, FileUploadError::NONE);
}

void FileManagementService::onFileUrlDownloadAbort(const std::string& deviceKey,
                                                   const FileUrlDownloadAbortMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    if (m_sessions.find(deviceKey) != m_sessions.cend() && m_sessions[deviceKey]->getUrl() == message.getPath())
        m_sessions[deviceKey]->abort();
}

void FileManagementService::onFileListRequest(const std::string& deviceKey,
                                              const FileListRequestMessage& /** message **/)
{
    LOG(TRACE) << METHOD_INFO;
    reportPresentFiles(deviceKey);
}

void FileManagementService::onFileDelete(const std::string& deviceKey, const FileDeleteMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // Go through the list of files
    for (const auto& file : message.getFiles())
    {
        if (FileSystemUtils::deleteFile(FileSystemUtils::composePath(file, m_fileLocation)))
        {
            m_files.erase(file);
            notifyListenerRemovedFile(deviceKey, file);
        }
    }

    // And report the files back
    reportPresentFiles(deviceKey);
}

void FileManagementService::onFilePurge(const std::string& deviceKey, const FilePurgeMessage& /** message **/)
{
    LOG(TRACE) << METHOD_INFO;

    // Just delete all the contents of the file
    for (const auto& file : FileSystemUtils::listFiles(m_fileLocation))
    {
        if (FileSystemUtils::deleteFile(FileSystemUtils::composePath(file, m_fileLocation)))
        {
            m_files.erase(file);
            notifyListenerRemovedFile(deviceKey, file);
        }
    }

    // And report the files back
    reportPresentFiles(deviceKey);
}

void FileManagementService::reportStatus(const std::string& deviceKey, FileUploadStatus status, FileUploadError error)
{
    LOG(TRACE) << METHOD_INFO;

    // Turn back around if there is no session for the device key
    if (m_sessions.find(deviceKey) == m_sessions.cend())
        return;

    // Make the message
    auto parsedMessage = std::shared_ptr<Message>{};
    if (m_sessions[deviceKey]->isPlatformTransfer())
    {
        auto fileName = m_sessions[deviceKey]->getName();
        auto message = FileUploadStatusMessage(fileName, status, error);
        parsedMessage = m_protocol.makeOutboundMessage(deviceKey, message);
    }
    else
    {
        auto filePath = m_sessions[deviceKey]->getUrl();
        auto fileName = m_sessions[deviceKey]->getName();
        auto message = FileUrlDownloadStatusMessage(filePath, fileName, status, error);
        parsedMessage = m_protocol.makeOutboundMessage(deviceKey, message);
    }

    // And publish it out
    if (parsedMessage != nullptr)
        m_connectivityService.publish(parsedMessage);
}

void FileManagementService::sendChunkRequest(const std::string& deviceKey, const FileBinaryRequestMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // Transform it into a protocol message and send out
    auto parsedMessage = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, message));
    m_connectivityService.publish(parsedMessage);
}

void FileManagementService::onFileSessionStatus(const std::string& deviceKey, FileUploadStatus status,
                                                FileUploadError error)
{
    LOG(TRACE) << METHOD_INFO;

    // Report the status
    reportStatus(deviceKey, status, error);

    // If the status is that the file is ready, or it is an error, stop the session
    if (status == FileUploadStatus::FILE_READY || status == FileUploadStatus::ERROR ||
        status == FileUploadStatus::ABORTED)
    {
        // If the file is ready, create the file
        if (status == FileUploadStatus::FILE_READY)
        {
            // Collect the chunks and place the file in
            const auto& fileName = m_sessions[deviceKey]->getName();

            // Get the absolute path for the file
            auto deviceFolder = FileSystemUtils::composePath(m_sessions[deviceKey]->getDeviceKey(), m_fileLocation);
            if (!FileSystemUtils::isDirectoryPresent(deviceFolder))
                FileSystemUtils::createDirectory(deviceFolder);
            auto relativePath = FileSystemUtils::composePath(fileName, deviceFolder);

            // Collect all the bytes for the file
            auto content = ByteArray{};
            if (m_sessions[deviceKey]->isPlatformTransfer())
            {
                const auto& chunks = m_sessions[deviceKey]->getChunks();
                for (const auto& chunk : chunks)
                    content.insert(content.end(), chunk.bytes.cbegin(), chunk.bytes.cend());
            }
            else
            {
                content = m_downloader->getBytes();
            }

            // Place the file in the folder
            if (!FileSystemUtils::createBinaryFileWithContent(relativePath, content))
                LOG(ERROR) << "Failed to store the '" << fileName << "' locally.";
            else
                notifyListenerAddedFile(deviceKey, fileName, absolutePathOfFile(deviceKey, fileName));
        }

        // Queue the session deletion
        m_commandBuffer.pushCommand(
          std::make_shared<std::function<void()>>([this, deviceKey] { m_sessions[deviceKey].reset(); }));
    }
}

FileInformation FileManagementService::obtainFileInformation(const std::string& deviceKey, const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Load up all the content of the file quickly
    auto binaryContent = ByteArray{};
    if (!FileSystemUtils::readBinaryFileContent(
          FileSystemUtils::composePath(fileName, FileSystemUtils::composePath(deviceKey, m_fileLocation)),
          binaryContent))
    {
        LOG(ERROR) << "Failed to obtain FileInformation for file '" << fileName
                   << "' -> Failed to read binary content of file.";
        return {};
    }

    // Compose the FileInformation based on the FileInformation
    const auto size = binaryContent.size();
    const auto hashBytes = ByteUtils::hashSHA256(binaryContent);
    auto stream = std::stringstream{};
    stream << std::hex << std::setfill('0');
    for (const auto& hashByte : hashBytes)
        stream << std::setw(2) << static_cast<std::int32_t>(hashByte);
    const auto hash = stream.str();
    return {fileName, size, hash};
}

void FileManagementService::reportTransferProtocolDisabled(const std::string& deviceKey, const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Form the message
    auto status =
      FileUploadStatusMessage{fileName, FileUploadStatus::ERROR, FileUploadError::TRANSFER_PROTOCOL_DISABLED};
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, status));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to report that transfer protocol is disabled -> Failed to make outbound status message.";
        return;
    }
    m_connectivityService.publish(message);
}

void FileManagementService::reportUrlTransferProtocolDisabled(const std::string& deviceKey, const std::string& url)
{
    LOG(TRACE) << METHOD_INFO;

    // Form the message
    auto status =
      FileUrlDownloadStatusMessage{url, "", FileUploadStatus::ERROR, FileUploadError::TRANSFER_PROTOCOL_DISABLED};
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, status));
    if (message == nullptr)
    {
        LOG(ERROR)
          << "Failed to report that url transfer protocol is disabled -> Failed to make outbound status message.";
        return;
    }
    m_connectivityService.publish(message);
}

std::string FileManagementService::absolutePathOfFile(const std::string& deviceKey, const std::string& file)
{
    LOG(TRACE) << METHOD_INFO;
    return FileSystemUtils::absolutePath(
      FileSystemUtils::composePath(file, FileSystemUtils::composePath(deviceKey, m_fileLocation)));
}

void FileManagementService::notifyListenerAddedFile(const std::string& deviceKey, const std::string& fileName,
                                                    const std::string& absolutePath)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if a listener exists
    if (auto listener = m_fileListener.lock())
    {
        if (listener != nullptr)
        {
            m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(
              [listener, deviceKey, fileName, absolutePath]()
              {
                  if (listener != nullptr)
                      listener->onAddedFile(deviceKey, fileName, absolutePath);
              }));
        }
    }
}

void FileManagementService::notifyListenerRemovedFile(const std::string& deviceKey, const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if a listener exists
    if (auto listener = m_fileListener.lock())
    {
        if (listener != nullptr)
        {
            m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(
              [listener, deviceKey, fileName]()
              {
                  if (listener != nullptr)
                      listener->onRemovedFile(deviceKey, fileName);
              }));
        }
    }
}
}    // namespace wolkabout

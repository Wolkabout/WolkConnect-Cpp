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

#include "core/model/MqttMessage.h"
#include "core/model/messages/ParametersUpdateMessage.h"
#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"
#include "wolk/service/file_management/FileManagementService.h"

#include <algorithm>
#include <utility>
#include <iomanip>

namespace wolkabout
{
FileManagementService::FileManagementService(std::string deviceKey, ConnectivityService& connectivityService,
                                             DataService& dataService, FileManagementProtocol& protocol,
                                             std::string fileLocation, bool fileTransferEnabled,
                                             bool fileTransferUrlEnabled, std::uint64_t maxPacketSize,
                                             std::shared_ptr<FileListener> fileListener)
: m_connectivityService(connectivityService)
, m_dataService(dataService)
, m_deviceKey(std::move(deviceKey))
, m_fileTransferEnabled(fileTransferEnabled)
, m_fileTransferUrlEnabled(fileTransferUrlEnabled)
, m_protocol(protocol)
, m_fileLocation(std::move(fileLocation))
, m_maxPacketSize(maxPacketSize)
, m_fileListener(fileListener)
{
    if (!(fileTransferEnabled || fileTransferUrlEnabled))
        throw std::runtime_error("Failed to create 'FileManagementService' with both flags disabled.");
}

void FileManagementService::setFileListener(const std::shared_ptr<FileListener>& fileListener)
{
    m_fileListener = fileListener;
}

void FileManagementService::onBuild()
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the folder by chance does not exist
    if (!FileSystemUtils::isDirectoryPresent(m_fileLocation))
    {
        FileSystemUtils::createDirectory(m_fileLocation);
        LOG(DEBUG) << "Created FileManagement directory '" << m_fileLocation << "'.";
    }
}

void FileManagementService::onConnected()
{
    LOG(TRACE) << METHOD_INFO;

    reportAllPresentFiles();
    reportParameters();
}

const Protocol& FileManagementService::getProtocol()
{
    return m_protocol;
}

void FileManagementService::messageReceived(std::shared_ptr<MqttMessage> message)
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
            if (m_fileTransferEnabled)
                onFileUploadInit(target, *parsedMessage);
            else
                reportTransferProtocolDisabled(parsedMessage->getName());
        else
            LOG(ERROR) << "Failed to parse incoming 'FileUploadInitiate' message.";
        return;
    }
    case MessageType::FILE_UPLOAD_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFileUploadAbort(message))
            if (m_fileTransferEnabled)
                onFileUploadAbort(target, *parsedMessage);
            else
                reportTransferProtocolDisabled(parsedMessage->getName());
        else
            LOG(ERROR) << "Failed to parse 'FileUploadAbort' message.";
        return;
    }
    case MessageType::FILE_BINARY_RESPONSE:
    {
        if (auto parsedMessage = m_protocol.parseFileBinaryResponse(message))
            if (m_fileTransferEnabled)
                onFileBinaryResponse(target, *parsedMessage);
            else
                return;    // We can't report the status, since this message does not carry the file name
        else
            LOG(ERROR) << "Failed to parse 'FileBinaryResponse' message.";
        return;
    }
    case MessageType::FILE_URL_DOWNLOAD_INIT:
    {
        if (auto parsedMessage = m_protocol.parseFileUrlDownloadInit(message))
            if (m_fileTransferUrlEnabled)
                onFileUrlDownloadInit(target, *parsedMessage);
            else
                reportUrlTransferProtocolDisabled(parsedMessage->getPath());
        else
            LOG(ERROR) << "Failed to parse 'FileUrlDownloadInit' message.";
        return;
    }
    case MessageType::FILE_URL_DOWNLOAD_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFileUrlDownloadAbort(message))
            if (m_fileTransferUrlEnabled)
                onFileUrlDownloadAbort(target, *parsedMessage);
            else
                reportUrlTransferProtocolDisabled(parsedMessage->getPath());
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

void FileManagementService::onFileListRequest(const std::string& /** deviceKey **/,
                                              const FileListRequestMessage& /** message **/)
{
    LOG(TRACE) << METHOD_INFO;
    reportAllPresentFiles();
}

void FileManagementService::onFileDelete(const std::string& /** deviceKey **/, const FileDeleteMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // Go through the list of files
    for (const auto& file : message.getFiles())
    {
        if (FileSystemUtils::deleteFile(FileSystemUtils::composePath(file, m_fileLocation)))
        {
            m_files.erase(file);
            notifyListenerRemovedFile(file);
        }
    }

    // And report the files back
    reportAllPresentFiles();
}

void FileManagementService::onFilePurge(const std::string& /** deviceKey **/, const FilePurgeMessage& /** message **/)
{
    LOG(TRACE) << METHOD_INFO;

    // Just delete all the contents of the file
    for (const auto& file : FileSystemUtils::listFiles(m_fileLocation))
    {
        if (FileSystemUtils::deleteFile(FileSystemUtils::composePath(file, m_fileLocation)))
        {
            m_files.erase(file);
            notifyListenerRemovedFile(file);
        }
    }

    // And report the files back
    reportAllPresentFiles();
}

FileInformation FileManagementService::obtainFileInformation(const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Load up all the content of the file quickly
    auto binaryContent = ByteArray{};
    if (!FileSystemUtils::readBinaryFileContent(FileSystemUtils::composePath(fileName, m_fileLocation), binaryContent))
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

void FileManagementService::reportAllPresentFiles()
{
    LOG(TRACE) << METHOD_INFO;

    // When the platform connection is established, we want to send out the current contents of the folder.
    auto folderContent = FileSystemUtils::listFiles(m_fileLocation);

    // Check if any files in the map are not in the folder anymore
    auto filesToErase = std::vector<std::string>{};
    for (const auto& pair : m_files)
    {
        const auto folderIt = std::find(folderContent.cbegin(), folderContent.cend(), pair.first);
        if (folderIt == folderContent.cend())
            filesToErase.emplace_back(pair.first);
    }
    for (const auto& fileToErase : filesToErase)
    {
        m_files.erase(fileToErase);
        LOG(DEBUG) << "Erased local FileInformation for file '" << fileToErase << "'.";
        notifyListenerRemovedFile(fileToErase);
    }

    auto fileInformationVector = std::vector<FileInformation>{};
    for (const auto& file : folderContent)
    {
        // Obtain information about the file
        auto informationIt = m_files.find(file);
        if (informationIt == m_files.cend())
        {
            // Look for it now
            auto freshInformation = obtainFileInformation(file);
            if (freshInformation.name.empty())
            {
                LOG(WARN) << "Failed to obtain FileInformation for file '" << file << "'.";
                continue;
            }

            // Emplace it in the map
            informationIt = m_files.emplace(file, freshInformation).first;
            LOG(DEBUG) << "Obtained local FileInformation for file '" << file << "'.";
            notifyListenerAddedFile(file, absolutePathOfFile(file));
        }

        // Emplace the complete information in the vector
        fileInformationVector.emplace_back(
          FileInformation{file, informationIt->second.size, informationIt->second.hash});
    }

    // Make the message
    auto fileList = FileListResponseMessage{fileInformationVector};
    auto message = std::shared_ptr<MqttMessage>(m_protocol.makeOutboundMessage(m_deviceKey, fileList));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to obtain serialized 'FileList' message.";
        return;
    }
    m_connectivityService.publish(message, false);
}

void FileManagementService::reportParameters()
{
    LOG(TRACE) << METHOD_INFO;

    // Form parameter messages based on flags
    auto transferParameter =
      Parameter{ParameterName::FILE_TRANSFER_PLATFORM_ENABLED, m_fileTransferEnabled ? "true" : "false"};
    auto urlTransferParameter =
      Parameter{ParameterName::FILE_TRANSFER_URL_ENABLED, m_fileTransferUrlEnabled ? "true" : "false"};

    // Update using the data service
    m_dataService.updateParameter(transferParameter);
    m_dataService.updateParameter(urlTransferParameter);
    m_dataService.publishParameters();
}

void FileManagementService::reportTransferProtocolDisabled(const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Form the message
    auto status =
      FileUploadStatusMessage{fileName, FileUploadStatus::ERROR, FileUploadError::TRANSFER_PROTOCOL_DISABLED};
    auto message = std::shared_ptr<MqttMessage>(m_protocol.makeOutboundMessage(m_deviceKey, status));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to report that transfer protocol is disabled -> Failed to make outbound status message.";
        return;
    }
    m_connectivityService.publish(message, false);
}

void FileManagementService::reportUrlTransferProtocolDisabled(const std::string& url)
{
    LOG(TRACE) << METHOD_INFO;

    // Form the message
    auto status =
      FileUrlDownloadStatusMessage{url, "", FileUploadStatus::ERROR, FileUploadError::TRANSFER_PROTOCOL_DISABLED};
    auto message = std::shared_ptr<MqttMessage>(m_protocol.makeOutboundMessage(m_deviceKey, status));
    if (message == nullptr)
    {
        LOG(ERROR)
          << "Failed to report that url transfer protocol is disabled -> Failed to make outbound status message.";
        return;
    }
    m_connectivityService.publish(message, false);
}

std::string FileManagementService::absolutePathOfFile(const std::string& file)
{
    LOG(TRACE) << METHOD_INFO;
    return FileSystemUtils::absolutePath(FileSystemUtils::composePath(file, m_fileLocation));
}

void FileManagementService::notifyListenerAddedFile(const std::string& fileName, const std::string& absolutePath)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if a listener exists
    if (auto listener = m_fileListener.lock())
    {
        m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(
          [listener, fileName, absolutePath]()
          {
              if (listener != nullptr)
                  listener->onAddedFile(fileName, absolutePath);
          }));
    }
}

void FileManagementService::notifyListenerRemovedFile(const std::string& fileName)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if a listener exists
    if (auto listener = m_fileListener.lock())
    {
        m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(
          [listener, fileName]()
          {
              if (listener != nullptr)
                  listener->onRemovedFile(fileName);
          }));
    }
}
}    // namespace wolkabout

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

#ifndef WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H
#define WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H

#include "core/InboundMessageHandler.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/protocol/FileManagementProtocol.h"
#include "core/utilities/CommandBuffer.h"
#include "wolk/api/FileListener.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/file_management/FileTransferSession.h"
#include "wolk/service/file_management/FileDownloader.h"

namespace wolkabout
{
// Here we have a publicly available MQTT message size limit
const std::uint32_t MQTT_MAX_MESSAGE_SIZE = 268435455;

class FileManagementService : public MessageListener
{
public:
    FileManagementService(std::string deviceKey, ConnectivityService& connectivityService, DataService& dataService,
                          FileManagementProtocol& protocol, std::string fileLocation, bool fileTransferEnabled = true,
                          bool fileTransferUrlEnabled = true, std::uint64_t maxPacketSize = MQTT_MAX_MESSAGE_SIZE,
                          std::shared_ptr<FileDownloader> fileDownloader = nullptr,
                          const std::shared_ptr<FileListener>& fileListener = nullptr);

    void setDownloader(const std::shared_ptr<FileDownloader>& downloader);

    void onBuild();

    void onConnected();

    CommandBuffer& getCommandBuffer();

    const Protocol& getProtocol() override;

    void messageReceived(std::shared_ptr<Message> message) override;

private:
    void onFileUploadInit(const std::string& deviceKey, const FileUploadInitiateMessage& message);

    void onFileUploadAbort(const std::string& deviceKey, const FileUploadAbortMessage& message);

    void onFileBinaryResponse(const std::string& deviceKey, const FileBinaryResponseMessage& message);

    void onFileUrlDownloadInit(const std::string& deviceKey, const FileUrlDownloadInitMessage& message);

    void onFileUrlDownloadAbort(const std::string& deviceKey, const FileUrlDownloadAbortMessage& message);

    void onFileListRequest(const std::string& deviceKey, const FileListRequestMessage& message);

    void onFileDelete(const std::string& deviceKey, const FileDeleteMessage& message);

    void onFilePurge(const std::string& deviceKey, const FilePurgeMessage& message);

    /**
     * This is an internal method that should be invoked to report the status of a transfer session.
     *
     * @param status The new FileUploadStatus value.
     * @param error The new FileUploadError value.
     */
    void reportStatus(FileUploadStatus status, FileUploadError error = FileUploadError::NONE);

    /**
     * This is an internal method that should be invoked to send out a binary request message.
     *
     * @param message The FileBinaryRequest message requesting the next chunk.
     */
    void sendChunkRequest(const FileBinaryRequestMessage& message);

    /**
     * This is an internal method that should be invoked in the FileTransferSession callback.
     *
     * @param status The new FileUploadStatus value.
     * @param error The new FileUploadError value.
     */
    void onFileSessionStatus(FileUploadStatus status, FileUploadError error = FileUploadError::NONE);

    /**
     * This is an internal method that will load a file from the filesystem, to collect the `FileInformation` object.
     * This will determine the size and the hash of the file.
     *
     * @param fileName The name of the file in the folder for which the information is needed.
     * @return The information that has been obtained about the file. If an error has occurred, an object with an empty
     * name will be returned.
     */
    FileInformation obtainFileInformation(const std::string& fileName);

    /**
     * This is an internal method that will cycle through all the files in the file system, collect the
     * FileInformation data about all of them (locally, or look into files to obtain and store locally), and send out a
     * `FileList` message with all of that data.
     */
    void reportAllPresentFiles();

    /**
     * This is an internal method that will report the parameters relating to the FileTransfer functionality to the
     * platform.
     */
    void reportParameters();

    /**
     * This is an internal method that can be quickly used to report that the regular transfer protocol is disabled.
     *
     * @param fileName The name of the file that the platform attempted to transfer.
     */
    void reportTransferProtocolDisabled(const std::string& fileName);

    /**
     * This is an internal method that can be quickly used to report that the url transfer protocol is disabled.
     *
     * @param url The url that the platform has sent out to the device to download a file from.
     */
    void reportUrlTransferProtocolDisabled(const std::string& url);

    /**
     * This is an internal method meant to make a shortcut to obtaining the absolute path for a file by pre-including
     * the `m_fileLocation` variable.
     *
     * @param file The name of the file in the folder.
     * @return The absolute path of a file.
     */
    std::string absolutePathOfFile(const std::string& file);

    /**
     * This is an internal method that will check if there exists a file listener, and if it does, it will queue up a
     * task to notify it of a newly added file.
     *
     * @param fileName The name of the newly added file.
     * @param absolutePath The absolute path of the newly added file.
     */
    void notifyListenerAddedFile(const std::string& fileName, const std::string& absolutePath);

    /**
     * This is an internal method that will check if there exists a file listener, and if it does, it will queue up a
     * task to notify it of a removed file.
     *
     * @param fileName The name of the removed file.
     */
    void notifyListenerRemovedFile(const std::string& fileName);

    // This is where we store the message sender and the device key information
    ConnectivityService& m_connectivityService;
    DataService& m_dataService;
    const std::string m_deviceKey;

    // These are the indicators of which modules of the FileManagement functionality are enabled.
    bool m_fileTransferEnabled;
    bool m_fileTransferUrlEnabled;

    // This is where the protocol will be passed while the service is created.
    FileManagementProtocol& m_protocol;

    // This is where the user parameters will be passed.
    std::string m_fileLocation;
    std::uint64_t m_maxPacketSize;

    // This is where we locally store information about files in memory
    std::map<std::string, FileInformation> m_files;

    // And here we place the ongoing session
    std::unique_ptr<FileTransferSession> m_session;

    // This is a pointer to a file downloader that we will use. In case that is supported.
    std::shared_ptr<FileDownloader> m_downloader;

    // Make place for the listener pointer
    std::weak_ptr<FileListener> m_fileListener;
    CommandBuffer m_commandBuffer;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H

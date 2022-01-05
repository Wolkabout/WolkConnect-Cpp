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
#include "wolk/service/file_management/FileDownloader.h"
#include "wolk/service/file_management/FileTransferSession.h"

namespace wolkabout
{
// Here we have an alias for a map of files stored for a single device
using DeviceFiles = std::map<std::string, FileInformation>;

class FileManagementService : public MessageListener
{
public:
    FileManagementService(ConnectivityService& connectivityService, DataService& dataService,
                          FileManagementProtocol& protocol, std::string fileLocation, bool fileTransferEnabled = true,
                          bool fileTransferUrlEnabled = true, std::shared_ptr<FileDownloader> fileDownloader = nullptr,
                          std::shared_ptr<FileListener> fileListener = nullptr);

    const Protocol& getProtocol() override;

    bool isFileTransferEnabled() const;

    bool isFileTransferUrlEnabled() const;

    /**
     * This is a createFolder method that should be invoked to loadState the folder for the FileManagement service.
     */
    virtual void createFolder();

    /**
     * This is a method that will report present files for a device.
     *
     * @param deviceKey The device for which the service will report files.
     */
    virtual void reportPresentFiles(const std::string& deviceKey);

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
     * @param deviceKey The device key for which the status is reported.
     * @param status The new FileUploadStatus value.
     * @param error The new FileUploadError value.
     */
    void reportStatus(const std::string& deviceKey, FileUploadStatus status,
                      FileUploadError error = FileUploadError::NONE);

    /**
     * This is an internal method that should be invoked to send out a binary request message.
     *
     * @param deviceKey The device key for which the request is being sent out.
     * @param message The FileBinaryRequest message requesting the next chunk.
     */
    void sendChunkRequest(const std::string& deviceKey, const FileBinaryRequestMessage& message);

    /**
     * This is an internal method that should be invoked in the FileTransferSession callback.
     *
     * @param status The new FileUploadStatus value.
     * @param error The new FileUploadError value.
     */
    void onFileSessionStatus(const std::string& deviceKey, FileUploadStatus status,
                             FileUploadError error = FileUploadError::NONE);

    /**
     * This is an internal method that will load a file from the filesystem, to collect the `FileInformation` object.
     * This will determine the size and the hash of the file.
     *
     * @param deviceKey The device key to which the file belongs.
     * @param fileName The name of the file in the folder for which the information is needed.
     * @return The information that has been obtained about the file. If an error has occurred, an object with an empty
     * name will be returned.
     */
    FileInformation obtainFileInformation(const std::string& deviceKey, const std::string& fileName);

    /**
     * This is an internal method that can be quickly used to report that the regular transfer protocol is disabled.
     *
     * @param deviceKey The device for which this is being reported.
     * @param fileName The name of the file that the platform attempted to transfer.
     */
    void reportTransferProtocolDisabled(const std::string& deviceKey, const std::string& fileName);

    /**
     * This is an internal method that can be quickly used to report that the url transfer protocol is disabled.
     *
     * @param deviceKey The device for which this is being reported.
     * @param url The url that the platform has sent out to the device to download a file from.
     */
    void reportUrlTransferProtocolDisabled(const std::string& deviceKey, const std::string& url);

    /**
     * This is an internal method meant to make a shortcut to obtaining the absolute path for a file by pre-including
     * the `m_fileLocation` variable.
     *
     * @param deviceKey The device key to which the file belongs.
     * @param file The name of the file in the folder.
     * @return The absolute path of a file.
     */
    std::string absolutePathOfFile(const std::string& deviceKey, const std::string& file);

    /**
     * This is an internal method that will check if there exists a file listener, and if it does, it will queue up a
     * task to notify it of a newly added file.
     *
     * @param deviceKey The device key for which the file got added.
     * @param fileName The name of the newly added file.
     * @param absolutePath The absolute path of the newly added file.
     */
    void notifyListenerAddedFile(const std::string& deviceKey, const std::string& fileName,
                                 const std::string& absolutePath);

    /**
     * This is an internal method that will check if there exists a file listener, and if it does, it will queue up a
     * task to notify it of a removed file.
     *
     * @param deviceKey The device key for which the file got removed.
     * @param fileName The name of the removed file.
     */
    void notifyListenerRemovedFile(const std::string& deviceKey, const std::string& fileName);

    // This is where we store the message sender and the device key information
    ConnectivityService& m_connectivityService;
    DataService& m_dataService;

    // These are the indicators of which modules of the FileManagement functionality are enabled.
    bool m_fileTransferEnabled;
    bool m_fileTransferUrlEnabled;

    // This is where the protocol will be passed while the service is created.
    FileManagementProtocol& m_protocol;

    // This is where the user parameters will be passed.
    std::string m_fileLocation;

    // This is where we locally store information about files in memory
    std::map<std::string, DeviceFiles> m_files;

    // And here we place the ongoing sessions
    std::map<std::string, std::unique_ptr<FileTransferSession>> m_sessions;

    // This is a pointer to a file downloader that we will use. In case that is supported.
    std::shared_ptr<FileDownloader> m_downloader;

    // Make place for the listener pointer
    std::weak_ptr<FileListener> m_fileListener;
    CommandBuffer m_commandBuffer;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H

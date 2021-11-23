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

#include "core/connectivity/ConnectivityService.h"
#include "core/InboundMessageHandler.h"
#include "core/protocol/FileManagementProtocol.h"

namespace wolkabout
{
class FileManagementService : public MessageListener
{
public:
    FileManagementService(ConnectivityService& connectivityService, FileManagementProtocol& protocol,
                          std::string fileLocation, std::uint64_t maxPacketSize);

    void messageReceived(std::shared_ptr<Message> message) override;

    const Protocol& getProtocol() override;

private:
    void onFileUploadInit(const std::string& deviceKey, const FileUploadInitiateMessage& message);

    void onFileUploadAbort(const std::string& deviceKey, const FileUploadAbortMessage& message);

    void onFileBinaryResponse(const std::string& deviceKey, const FileBinaryResponseMessage& message);

    void onFileUrlDownloadInit(const std::string& deviceKey, const FileUrlDownloadInitMessage& message);

    void onFileUrlDownloadAbort(const std::string& deviceKey, const FileUrlDownloadAbortMessage& message);

    void onFileListRequest(const std::string& deviceKey, const FileListRequestMessage& message);

    void onFileDelete(const std::string& deviceKey, const FileDeleteMessage& message);

    void onFilePurge(const std::string& deviceKey, const FilePurgeMessage& message);

    // This is where we store the message sender
    ConnectivityService& m_connectivityService;

    // This is where the protocol will be passed while the service is created.
    FileManagementProtocol& m_protocol;

    // This is where the user parameters will be passed.
    std::string m_fileLocation;
    std::uint64_t m_maxPacketSize;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H

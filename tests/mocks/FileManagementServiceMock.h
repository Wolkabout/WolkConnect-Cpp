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

#ifndef WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICEMOCK_H
#define WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICEMOCK_H

#include "wolk/service/file_management/FileManagementService.h"

#include <gmock/gmock.h>

#include <utility>

using namespace wolkabout::connect;

class FileManagementServiceMock : public FileManagementService
{
public:
    FileManagementServiceMock(ConnectivityService& connectivityService, DataService& dataService,
                              FileManagementProtocol& protocol, std::string fileLocation,
                              bool fileTransferEnabled = true, bool fileTransferUrlEnabled = true,
                              std::shared_ptr<FileDownloader> fileDownloader = nullptr,
                              const std::shared_ptr<FileListener>& fileListener = nullptr)
    : FileManagementService(connectivityService, dataService, protocol, std::move(fileLocation), fileTransferEnabled,
                            fileTransferUrlEnabled, std::move(fileDownloader), fileListener)
    {
    }
    MOCK_METHOD(const Protocol&, getProtocol, ());
    MOCK_METHOD(void, createFolder, ());
    MOCK_METHOD(void, reportPresentFiles, (const std::string&));
    MOCK_METHOD(void, messageReceived, (std::shared_ptr<Message>));
};

#endif    // WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICEMOCK_H

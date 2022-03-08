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

#ifndef WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICEMOCK_H
#define WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICEMOCK_H

#include "wolk/service/firmware_update/FirmwareUpdateService.h"

#include <gmock/gmock.h>

using namespace wolkabout;
using namespace wolkabout::connect;

class FirmwareUpdateServiceMock : public FirmwareUpdateService
{
public:
    FirmwareUpdateServiceMock(ConnectivityService& connectivityService, DataService& dataService,
                              std::shared_ptr<FileManagementService> fileManagementService,
                              std::unique_ptr<FirmwareInstaller> firmwareInstaller, FirmwareUpdateProtocol& protocol,
                              const std::string& workingDirectory = "./")
    : FirmwareUpdateService(connectivityService, dataService, std::move(fileManagementService),
                            std::move(firmwareInstaller), protocol, workingDirectory)
    {
    }
    FirmwareUpdateServiceMock(ConnectivityService& connectivityService, DataService& dataService,
                              std::shared_ptr<FileManagementService> fileManagementService,
                              std::unique_ptr<FirmwareParametersListener> firmwareParametersListener,
                              FirmwareUpdateProtocol& protocol, const std::string& workingDirectory = "./")
    : FirmwareUpdateService(connectivityService, dataService, std::move(fileManagementService),
                            std::move(firmwareParametersListener), protocol, workingDirectory)
    {
    }
    MOCK_METHOD(void, loadState, (const std::string&));
    MOCK_METHOD(void, obtainParametersAndAnnounce, (const std::string&));
    MOCK_METHOD(std::string, getVersionForDevice, (const std::string& deviceKey));
};

#endif    // WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICEMOCK_H

/**
 * Copyright 2022 Wolkabout Technology s.r.o.
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

#ifndef WOLKGATEWAY_FIRMWAREINSTALLERMOCK_H
#define WOLKGATEWAY_FIRMWAREINSTALLERMOCK_H

#include "wolk/api/FirmwareInstaller.h"

#include <gmock/gmock.h>

using namespace wolkabout::connect;

class FirmwareInstallerMock : public FirmwareInstaller
{
public:
    MOCK_METHOD(InstallResponse, installFirmware, (const std::string&, const std::string&));
    MOCK_METHOD(void, abortFirmwareInstall, (const std::string&));
    MOCK_METHOD(bool, wasFirmwareInstallSuccessful, (const std::string&, const std::string&));
    MOCK_METHOD(std::string, getFirmwareVersion, (const std::string&));
};

#endif    // WOLKGATEWAY_FIRMWAREINSTALLERMOCK_H

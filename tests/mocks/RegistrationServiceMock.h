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

#ifndef WOLKABOUTCONNECTOR_REGISTRATIONSERVICEMOCK_H
#define WOLKABOUTCONNECTOR_REGISTRATIONSERVICEMOCK_H

#include "wolk/service/registration_service/RegistrationService.h"

#include <gmock/gmock.h>

using namespace wolkabout;
using namespace wolkabout::connect;

class RegistrationServiceMock : public RegistrationService
{
public:
    RegistrationServiceMock(RegistrationProtocol& protocol, ConnectivityService& connectivityService,
                            ErrorService& errorService)
    : RegistrationService(protocol, connectivityService, errorService)
    {
    }
    MOCK_METHOD(std::unique_ptr<ErrorMessage>, registerDevices,
                (const std::string&, const std::vector<DeviceRegistrationData>&, std::chrono::milliseconds));
    MOCK_METHOD(std::unique_ptr<ErrorMessage>, removeDevices,
                (const std::string&, std::vector<std::string>, std::chrono::milliseconds));
    MOCK_METHOD(std::unique_ptr<std::vector<RegisteredDeviceInformation>>, obtainDevices,
                (const std::string&, TimePoint, std::string, std::string, std::chrono::milliseconds));
    MOCK_METHOD(bool, obtainDevicesAsync,
                (const std::string&, TimePoint, std::string, std::string,
                 std::function<void(const std::vector<RegisteredDeviceInformation>&)>));
};

#endif    // WOLKABOUTCONNECTOR_REGISTRATIONSERVICEMOCK_H

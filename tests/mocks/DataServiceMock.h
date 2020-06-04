/*
 * Copyright 2020 WolkAbout Technology s.r.o.
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
#ifndef WOLKABOUTCONNECTOR_DATASERVICEMOCK_H
#define WOLKABOUTCONNECTOR_DATASERVICEMOCK_H

#include "service/DataService.h"

#include <gmock/gmock.h>

class DataServiceMock : public wolkabout::DataService
{
public:
    DataServiceMock(const std::string& deviceKey, wolkabout::DataProtocol& protocol,
                    wolkabout::Persistence& persistence, wolkabout::ConnectivityService& connectivityService)
    : DataService(deviceKey, protocol, persistence, connectivityService, nullptr, nullptr, nullptr, nullptr)
    {
    }

    MOCK_METHOD(void, addSensorReading, (const std::string&, const std::string&, unsigned long long int));
    MOCK_METHOD(void, addSensorReading, (const std::string&, const std::vector<std::string>&, unsigned long long int));
    MOCK_METHOD(void, addActuatorStatus, (const std::string&, const std::string&, wolkabout::ActuatorStatus::State));

    MOCK_METHOD(void, publishSensorReadings, ());
    MOCK_METHOD(void, publishActuatorStatuses, ());
    MOCK_METHOD(void, publishAlarms, ());
    MOCK_METHOD(void, publishConfiguration, ());
};

#endif    // WOLKABOUTCONNECTOR_DATASERVICEMOCK_H

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
#ifndef WOLKABOUTCONNECTOR_PERSISTENCEMOCK_H
#define WOLKABOUTCONNECTOR_PERSISTENCEMOCK_H

#include "persistence/Persistence.h"

#include <gmock/gmock.h>

class PersistenceMock : public wolkabout::Persistence
{
public:
    MOCK_METHOD2(putSensorReading, bool(const std::string&, std::shared_ptr<wolkabout::SensorReading>));
    MOCK_METHOD2(getSensorReadings,
                 std::vector<std::shared_ptr<wolkabout::SensorReading>>(const std::string&, std::uint_fast64_t));
    MOCK_METHOD2(removeSensorReadings, void(const std::string&, std::uint_fast64_t count));
    MOCK_METHOD0(getSensorReadingsKeys, std::vector<std::string>());
    MOCK_METHOD2(putAlarm, bool(const std::string&, std::shared_ptr<wolkabout::Alarm> alarm));
    MOCK_METHOD2(getAlarms,
                 std::vector<std::shared_ptr<wolkabout::Alarm>>(const std::string&, std::uint_fast64_t count));
    MOCK_METHOD2(removeAlarms, void(const std::string&, std::uint_fast64_t count));
    MOCK_METHOD0(getAlarmsKeys, std::vector<std::string>());
    MOCK_METHOD2(putActuatorStatus,
                 bool(const std::string&, std::shared_ptr<wolkabout::ActuatorStatus> actuatorStatus));
    MOCK_METHOD1(getActuatorStatus, std::shared_ptr<wolkabout::ActuatorStatus>(const std::string&));
    MOCK_METHOD1(removeActuatorStatus, void(const std::string&));
    MOCK_METHOD0(getActuatorStatusesKeys, std::vector<std::string>());
    MOCK_METHOD2(putConfiguration,
                 bool(const std::string&, std::shared_ptr<std::vector<wolkabout::ConfigurationItem>> configuration));
    MOCK_METHOD1(getConfiguration, std::shared_ptr<std::vector<wolkabout::ConfigurationItem>>(const std::string&));
    MOCK_METHOD1(removeConfiguration, void(const std::string&));
    MOCK_METHOD0(getConfigurationKeys, std::vector<std::string>());
    MOCK_METHOD0(isEmpty, bool());
};

#endif    // WOLKABOUTCONNECTOR_PERSISTENCEMOCK_H

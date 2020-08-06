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
#ifndef WOLKABOUTCONNECTOR_DATAPROTOCOLMOCK_H
#define WOLKABOUTCONNECTOR_DATAPROTOCOLMOCK_H

#include "protocol/DataProtocol.h"

#include <gmock/gmock.h>

class DataProtocolMock : public wolkabout::DataProtocol
{
public:
    MOCK_CONST_METHOD1(extractReferenceFromChannel, std::string(const std::string&));

    MOCK_CONST_METHOD1(isActuatorSetMessage, bool(const wolkabout::Message&));
    MOCK_CONST_METHOD1(isActuatorGetMessage, bool(const wolkabout::Message&));

    MOCK_CONST_METHOD1(isConfigurationSetMessage, bool(const wolkabout::Message&));
    MOCK_CONST_METHOD1(isConfigurationGetMessage, bool(const wolkabout::Message&));

    MOCK_CONST_METHOD1(makeActuatorGetCommand,
                       std::unique_ptr<wolkabout::ActuatorGetCommand>(const wolkabout::Message&));
    MOCK_CONST_METHOD1(makeActuatorSetCommand,
                       std::unique_ptr<wolkabout::ActuatorSetCommand>(const wolkabout::Message&));

    MOCK_CONST_METHOD1(makeConfigurationSetCommand,
                       std::unique_ptr<wolkabout::ConfigurationSetCommand>(const wolkabout::Message&));

    MOCK_CONST_METHOD2(
      makeMessage, std::unique_ptr<wolkabout::Message>(const std::string&,
                                                       const std::vector<std::shared_ptr<wolkabout::SensorReading>>&));

    MOCK_CONST_METHOD2(makeMessage,
                       std::unique_ptr<wolkabout::Message>(const std::string&,
                                                           const std::vector<std::shared_ptr<wolkabout::Alarm>>&));

    MOCK_CONST_METHOD2(
      makeMessage, std::unique_ptr<wolkabout::Message>(const std::string&,
                                                       const std::vector<std::shared_ptr<wolkabout::ActuatorStatus>>&));

    MOCK_CONST_METHOD2(makeMessage,
                       std::unique_ptr<wolkabout::Message>(const std::string&,
                                                           const std::vector<wolkabout::ConfigurationItem>&));

    MOCK_CONST_METHOD0(getInboundChannels, std::vector<std::string>());
    MOCK_CONST_METHOD1(getInboundChannelsForDevice, std::vector<std::string>(const std::string&));
    MOCK_CONST_METHOD1(extractDeviceKeyFromChannel, std::string(const std::string&));
};

#endif    // WOLKABOUTCONNECTOR_DATAPROTOCOLMOCK_H

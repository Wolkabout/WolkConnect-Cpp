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
#ifndef WOLKABOUTCONNECTOR_STATUSPROTOCOLMOCK_H
#define WOLKABOUTCONNECTOR_STATUSPROTOCOLMOCK_H

#include "protocol/StatusProtocol.h"

class StatusProtocolMock : public wolkabout::StatusProtocol
{
public:
    StatusProtocolMock() = default;

    MOCK_METHOD(bool, isStatusRequestMessage, (const wolkabout::Message& message), (override));
    MOCK_METHOD(bool, isStatusConfirmMessage, (const wolkabout::Message& message), (override));
    MOCK_METHOD(bool, isPongMessage, (const wolkabout::Message& message), (override));
    MOCK_METHOD(std::unique_ptr<wolkabout::Message>, makeStatusResponseMessage,
                (const std::string& deviceKey, const wolkabout::DeviceStatus& response), (override));
    MOCK_METHOD(std::unique_ptr<wolkabout::Message>, makeStatusUpdateMessage,
                (const std::string& deviceKey, const wolkabout::DeviceStatus& response), (override));
    MOCK_METHOD(std::unique_ptr<wolkabout::Message>, makeLastWillMessage, (const std::string& deviceKey), (override));
    MOCK_METHOD(std::unique_ptr<wolkabout::Message>, makeLastWillMessage, (const std::vector<std::string>& deviceKeys), (override));
    MOCK_METHOD(std::unique_ptr<wolkabout::Message>, makeFromPingRequest, (const std::string& deviceKey), (override));

    MOCK_METHOD(std::vector<std::string>, getInboundChannels, (), (override));
    MOCK_METHOD(std::vector<std::string>, getInboundChannelsForDevice, (const std::string& deviceKey), (override));
    MOCK_METHOD(std::string, extractDeviceKeyFromChannel, (const std::string& topic), (override));
};

#endif    // WOLKABOUTCONNECTOR_STATUSPROTOCOLMOCK_H

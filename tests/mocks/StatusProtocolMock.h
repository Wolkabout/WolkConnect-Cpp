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
#ifndef WOLKABOUTCONNECTOR_STATUSPROTOCOLMOCK_H
#define WOLKABOUTCONNECTOR_STATUSPROTOCOLMOCK_H

#include "protocol/StatusProtocol.h"

#include <gmock/gmock.h>

class StatusProtocolMock : public wolkabout::StatusProtocol
{
public:
    StatusProtocolMock() = default;

    MOCK_CONST_METHOD1(isStatusRequestMessage, bool(const wolkabout::Message&));
    MOCK_CONST_METHOD1(isStatusConfirmMessage, bool(const wolkabout::Message&));
    MOCK_CONST_METHOD1(isPongMessage, bool(const wolkabout::Message&));

    MOCK_CONST_METHOD2(makeStatusResponseMessage,
                       std::unique_ptr<wolkabout::Message>(const std::string&, const wolkabout::DeviceStatus&));
    MOCK_CONST_METHOD2(makeStatusUpdateMessage,
                       std::unique_ptr<wolkabout::Message>(const std::string&, const wolkabout::DeviceStatus&));
    MOCK_CONST_METHOD1(makeLastWillMessage, std::unique_ptr<wolkabout::Message>(const std::string&));
    MOCK_CONST_METHOD1(makeLastWillMessage, std::unique_ptr<wolkabout::Message>(const std::vector<std::string>&));
    MOCK_CONST_METHOD1(makeFromPingRequest, std::unique_ptr<wolkabout::Message>(const std::string&));

    MOCK_CONST_METHOD0(getInboundChannels, std::vector<std::string>());
    MOCK_CONST_METHOD1(getInboundChannelsForDevice, std::vector<std::string>(const std::string&));
    MOCK_CONST_METHOD1(extractDeviceKeyFromChannel, std::string(const std::string&));
};

#endif    // WOLKABOUTCONNECTOR_STATUSPROTOCOLMOCK_H

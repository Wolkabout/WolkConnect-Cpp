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
#ifndef WOLKABOUTCONNECTOR_PROTOCOLMOCK_H
#define WOLKABOUTCONNECTOR_PROTOCOLMOCK_H

#include "protocol/Protocol.h"

#include <gmock/gmock.h>

class ProtocolMock : public wolkabout::Protocol
{
public:
    ProtocolMock() = default;
    virtual ~ProtocolMock() {}

    MOCK_METHOD(std::vector<std::string>, getInboundChannels, (), (override, const));
    MOCK_METHOD(std::vector<std::string>, getInboundChannelsForDevice, (const std::string&), (override, const));
    MOCK_METHOD(std::string, extractDeviceKeyFromChannel, (const std::string&), (override, const));
};

#endif    // WOLKABOUTCONNECTOR_PROTOCOLMOCK_H

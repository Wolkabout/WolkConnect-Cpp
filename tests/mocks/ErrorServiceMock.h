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

#ifndef WOLKABOUTCONNECTOR_ERRORSERVICEMOCK_H
#define WOLKABOUTCONNECTOR_ERRORSERVICEMOCK_H

#include "wolk/service/error/ErrorService.h"

#include <gmock/gmock.h>

using namespace wolkabout::connect;

class ErrorServiceMock : public ErrorService
{
public:
    ErrorServiceMock(ErrorProtocol& protocol, const std::chrono::milliseconds& retainTime)
    : ErrorService(protocol, retainTime)
    {
    }
    MOCK_METHOD(void, start, ());
    MOCK_METHOD(void, stop, ());
    MOCK_METHOD(std::uint64_t, peekMessagesForDevice, (const std::string&));
    MOCK_METHOD(std::unique_ptr<ErrorMessage>, obtainFirstMessageForDevice, (const std::string&));
    MOCK_METHOD(std::unique_ptr<ErrorMessage>, obtainLastMessageForDevice, (const std::string&));
    MOCK_METHOD(bool, awaitMessage, (const std::string&, std::chrono::milliseconds));
    MOCK_METHOD(std::unique_ptr<ErrorMessage>, obtainOrAwaitMessageForDevice,
                (const std::string&, std::chrono::milliseconds));
    MOCK_METHOD(void, messageReceived, (std::shared_ptr<Message>));
    MOCK_METHOD(const Protocol&, getProtocol, ());
};

#endif    // WOLKABOUTCONNECTOR_ERRORSERVICEMOCK_H

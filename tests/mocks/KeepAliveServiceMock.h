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
#ifndef WOLKABOUTCONNECTOR_KEEPALIVESERVICEMOCK_H
#define WOLKABOUTCONNECTOR_KEEPALIVESERVICEMOCK_H

#include "service/KeepAliveService.h"

class KeepAliveServiceMock: public wolkabout::KeepAliveService
{
public:
    KeepAliveServiceMock(const std::string& deviceKey, wolkabout::StatusProtocol& protocol,
                         wolkabout::ConnectivityService& connectivityService,
                         const std::chrono::seconds& keepAliveInterval)
    : KeepAliveService(deviceKey, protocol, connectivityService, keepAliveInterval)
    {
    }

    MOCK_METHOD(void, connected, ());
    MOCK_METHOD(void, disconnected, ());
};

#endif    // WOLKABOUTCONNECTOR_KEEPALIVESERVICEMOCK_H

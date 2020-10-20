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
#ifndef WOLKABOUTCONNECTOR_CONNECTIVITYSERVICEMOCK_H
#define WOLKABOUTCONNECTOR_CONNECTIVITYSERVICEMOCK_H

#include <gmock/gmock.h>

#include "connectivity/ConnectivityService.h"

class ConnectivityServiceMock : public wolkabout::ConnectivityService
{
public:
    ConnectivityServiceMock() = default;

    MOCK_METHOD0(connect, bool());
    MOCK_METHOD0(disconnect, void());
    MOCK_METHOD0(reconnect, bool());
    MOCK_METHOD0(isConnected, bool());
    MOCK_METHOD2(publish, bool(std::shared_ptr<wolkabout::Message>, bool));
    MOCK_METHOD2(setUncontrolledDisonnectMessage, void(std::shared_ptr<wolkabout::Message>, bool));
};

#endif    // WOLKABOUTCONNECTOR_CONNECTIVITYSERVICEMOCK_H

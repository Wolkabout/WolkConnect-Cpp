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
#ifndef WOLKABOUTCONNECTOR_CONFIGURATIONHANDLERMOCK_H
#define WOLKABOUTCONNECTOR_CONFIGURATIONHANDLERMOCK_H

#include "api/ConfigurationHandler.h"

#include <gmock/gmock.h>

class ConfigurationHandlerMock : public wolkabout::ConfigurationHandler
{
public:
    MOCK_METHOD(void, handleConfiguration, (const std::vector<wolkabout::ConfigurationItem>&), (override));
};

#endif    // WOLKABOUTCONNECTOR_CONFIGURATIONHANDLERMOCK_H

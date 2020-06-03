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
#ifndef WOLKABOUTCONNECTOR_CONFIGURATIONPROVIDERMOCK_H
#define WOLKABOUTCONNECTOR_CONFIGURATIONPROVIDERMOCK_H

#include "ConfigurationProvider.h"

#include <gmock/gmock.h>

class ConfigurationProviderMock: public wolkabout::ConfigurationProvider
{
public:
    MOCK_METHOD(std::vector<wolkabout::ConfigurationItem>, getConfiguration, (), (override));
};

#endif    // WOLKABOUTCONNECTOR_CONFIGURATIONPROVIDERMOCK_H

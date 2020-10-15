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
#ifndef WOLKABOUTCONNECTOR_ACTUATORSTATUSPROVIDERMOCK_H
#define WOLKABOUTCONNECTOR_ACTUATORSTATUSPROVIDERMOCK_H

#define private public
#define protected public
#include "api/ActuatorStatusProvider.h"
#undef private
#undef protected

#include <gmock/gmock.h>

class ActuatorStatusProviderMock : public wolkabout::ActuatorStatusProvider
{
public:
    MOCK_METHOD(wolkabout::ActuatorStatus, getActuatorStatus, (const std::string&), (override));
};

#endif    // WOLKABOUTCONNECTOR_ACTUATORSTATUSPROVIDERMOCK_H

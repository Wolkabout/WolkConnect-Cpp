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

#ifndef WOLKABOUTCONNECTOR_PARAMETERHANDLERMOCK_H
#define WOLKABOUTCONNECTOR_PARAMETERHANDLERMOCK_H

#include "wolk/api/ParameterHandler.h"

#include <gmock/gmock.h>

using namespace wolkabout::connect;

class ParameterHandlerMock : public ParameterHandler
{
public:
    MOCK_METHOD(void, handleUpdate, (const std::string&, const std::vector<Parameter>&));
};

#endif    // WOLKABOUTCONNECTOR_PARAMETERHANDLERMOCK_H

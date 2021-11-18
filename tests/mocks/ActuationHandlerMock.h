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
#ifndef WOLKABOUTCONNECTOR_ACTUATIONHANDLERMOCK_H
#define WOLKABOUTCONNECTOR_ACTUATIONHANDLERMOCK_H

#define private public
#define protected public
#include "api/FeedUpdateHandler.h"
#undef private
#undef protected

#include <gmock/gmock.h>

class ActuationHandlerMock : public wolkabout::ActuationHandler
{
public:
    MOCK_METHOD(void, handleActuation, (const std::string&, const std::string&), (override));
};

#endif    // WOLKABOUTCONNECTOR_ACTUATIONHANDLERMOCK_H

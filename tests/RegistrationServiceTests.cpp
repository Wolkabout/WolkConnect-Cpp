/**
 * Copyright 2021 Wolkabout s.r.o.
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

#include <any>
#include <sstream>

#define private public
#define protected public
#include "wolk/service/registration_service/RegistrationService.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/ErrorProtocolMock.h"
#include "tests/mocks/ErrorServiceMock.h"
#include "tests/mocks/RegistrationProtocolMock.h"

#include <gtest/gtest.h>

using namespace wolkabout;
using namespace ::testing;

class RegistrationServiceTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    void SetUp() override
    {
        errorServiceMock = std::make_shared<NiceMock<ErrorServiceMock>>(errorProtocolMock, RETAIN_TIME);
        service =
          std::unique_ptr<RegistrationService>{new RegistrationService{registrationProtocolMock, *errorServiceMock}};
    }

    void TearDown() override { service.reset(); }

    const std::chrono::milliseconds RETAIN_TIME = std::chrono::milliseconds{500};

    ErrorProtocolMock errorProtocolMock;

    RegistrationProtocolMock registrationProtocolMock;

    std::shared_ptr<ErrorServiceMock> errorServiceMock;

    std::unique_ptr<RegistrationService> service;
};

TEST_F(RegistrationServiceTests, HelloWorld) {}

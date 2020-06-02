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

#define private public
#define protected public
#include "protocol/DataProtocol.h"
#include "WolkBuilder.h"
#include "Wolk.h"
#undef private
#undef protected

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>

#include "mocks/ActuationHandlerMock.h"
#include "mocks/ActuatorStatusProviderMock.h"

class WolkBuilderTests : public ::testing::Test
{
};

TEST_F(WolkBuilderTests, CtorTests)
{
    const auto& testDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    EXPECT_NO_THROW(wolkabout::WolkBuilder deviceBuilder(*testDevice));
}

TEST_F(WolkBuilderTests, LambdaHandlers)
{
    const auto& testDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    bool handleSuccess = false, statusSuccess = false;

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    ASSERT_NO_THROW(builder->actuatorStatusProvider([&](const std::string& value) {
        std::cout << "ActuatorStatusProvider: " << value << std::endl;
        statusSuccess = true;
        return wolkabout::ActuatorStatus(value, wolkabout::ActuatorStatus::State::READY);
    }));

    ASSERT_NO_THROW(builder->actuationHandler([&](const std::string& key, const std::string& value) {
        std::cout << "ActuatorHandler: " << key << ", " << value << std::endl;
        handleSuccess = true;
    }));

    std::shared_ptr<wolkabout::Wolk> wolk = nullptr;
    EXPECT_NO_THROW(wolk = builder->build());

    wolk->handleActuatorSetCommand("KEY", "VALUE");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(handleSuccess);
    EXPECT_TRUE(statusSuccess);
}

TEST_F(WolkBuilderTests, MockHandlers)
{
    const auto& testDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    bool handleSuccess = false, statusSuccess = false;

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    std::shared_ptr<ActuatorStatusProviderMock> actuatorStatusMock;
    actuatorStatusMock.reset(new ::testing::NiceMock<ActuatorStatusProviderMock>());
    ASSERT_NO_THROW(builder->actuatorStatusProvider(actuatorStatusMock));

    EXPECT_CALL(*actuatorStatusMock, getActuatorStatus).WillOnce([&](const std::string& value) {
        statusSuccess = true;
        std::cout << "ActuatorStatusProvider: " << value << std::endl;
        return wolkabout::ActuatorStatus("VALUE", wolkabout::ActuatorStatus::State::READY);
    });

    std::shared_ptr<ActuationHandlerMock> actuationMock;
    actuationMock.reset(new ::testing::NiceMock<ActuationHandlerMock>());
    ASSERT_NO_THROW(builder->actuationHandler(actuationMock));

    EXPECT_CALL(*actuationMock, handleActuation).WillOnce([&](const std::string& key, const std::string& value) {
        std::cout << "ActuatorHandler: " << key << ", " << value << std::endl;
        handleSuccess = true;
    });

    std::shared_ptr<wolkabout::Wolk> wolk = nullptr;
    EXPECT_NO_THROW(wolk = builder->build());

    wolk->handleActuatorSetCommand("KEY", "VALUE");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(handleSuccess);
    EXPECT_TRUE(statusSuccess);
}

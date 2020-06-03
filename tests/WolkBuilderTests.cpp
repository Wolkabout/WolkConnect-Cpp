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
#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ConfigurationSetCommand.h"
#include "model/Message.h"
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
#include "mocks/ConfigurationHandlerMock.h"
#include "mocks/ConfigurationProviderMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/PersistenceMock.h"

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

    // For actuator
    bool actuatorHandleSuccess = false, actuatorStatusSuccess = false;
    // For configuration
    bool configurationHandlerSuccess = false, configurationProviderSuccess = false;

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    ASSERT_NO_THROW(builder->actuatorStatusProvider([&](const std::string& value) {
        std::cout << "ActuatorStatusProvider: " << value << std::endl;
        actuatorStatusSuccess = true;
        return wolkabout::ActuatorStatus(value, wolkabout::ActuatorStatus::State::READY);
    }));

    ASSERT_NO_THROW(builder->actuationHandler([&](const std::string& key, const std::string& value) {
        std::cout << "ActuatorHandler: " << key << ", " << value << std::endl;
        actuatorHandleSuccess = true;
    }));

    ASSERT_NO_THROW(builder->configurationHandler([&](const std::vector<wolkabout::ConfigurationItem>& items) {
        std::cout << "ConfigurationHandler: Size of items : " << items.size() << std::endl;
        if (items.size() == 3)
        {
            configurationHandlerSuccess = true;
        }
        else
        {
            std::cout << "ConfigurationHandler: Passed items map isn't of size 3!";
        }
    }));

    ASSERT_NO_THROW(builder->configurationProvider([&]() {
        std::cout << "ConfigurationProvider: Providing configurations!" << std::endl;
        configurationProviderSuccess = true;
        return std::vector<wolkabout::ConfigurationItem>();
    }));

    std::shared_ptr<wolkabout::Wolk> wolk = nullptr;
    EXPECT_NO_THROW(wolk = builder->build());

    wolk->handleActuatorSetCommand("KEY", "VALUE");

    const auto& configItem1 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2"}, "R1");
    const auto& configItem2 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1"}, "R2");
    const auto& configItem3 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2", "V3"}, "R3");
    const auto& configSetCommand = wolkabout::ConfigurationSetCommand(
      std::vector<wolkabout::ConfigurationItem>{configItem1, configItem2, configItem3});
    wolk->handleConfigurationSetCommand(configSetCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(actuatorHandleSuccess);
    EXPECT_TRUE(actuatorStatusSuccess);
}

TEST_F(WolkBuilderTests, MockHandlers)
{
    const auto& testDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    // For actuator
    bool actuatorHandleSuccess = false, actuatorStatusSuccess = false;
    // For configuration
    bool configurationHandlerSuccess = false, configurationProviderSuccess = false;

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    std::shared_ptr<ActuatorStatusProviderMock> actuatorStatusMock;
    actuatorStatusMock.reset(new ::testing::NiceMock<ActuatorStatusProviderMock>());
    ASSERT_NO_THROW(builder->actuatorStatusProvider(actuatorStatusMock));

    EXPECT_CALL(*actuatorStatusMock, getActuatorStatus).WillOnce([&](const std::string& value) {
        actuatorHandleSuccess = true;
        std::cout << "ActuatorStatusProvider: " << value << std::endl;
        return wolkabout::ActuatorStatus("VALUE", wolkabout::ActuatorStatus::State::READY);
    });

    std::shared_ptr<ActuationHandlerMock> actuationMock;
    actuationMock.reset(new ::testing::NiceMock<ActuationHandlerMock>());
    ASSERT_NO_THROW(builder->actuationHandler(actuationMock));

    EXPECT_CALL(*actuationMock, handleActuation).WillOnce([&](const std::string& key, const std::string& value) {
        std::cout << "ActuatorHandler: " << key << ", " << value << std::endl;
        actuatorStatusSuccess = true;
    });

    std::shared_ptr<ConfigurationHandlerMock> configurationHandlerMock;
    configurationHandlerMock.reset(new ::testing::NiceMock<ConfigurationHandlerMock>());
    ASSERT_NO_THROW(builder->configurationHandler(configurationHandlerMock));

    EXPECT_CALL(*configurationHandlerMock, handleConfiguration)
      .WillOnce([&](const std::vector<wolkabout::ConfigurationItem>& items) {
          std::cout << "ConfigurationHandler: Size of items : " << items.size() << std::endl;
          if (items.size() == 4)
          {
              configurationHandlerSuccess = true;
          }
          else
          {
              std::cout << "ConfigurationHandler: Passed items map isn't of size 4!";
          }
      });

    std::shared_ptr<ConfigurationProviderMock> configurationProviderMock;
    configurationProviderMock.reset(new ::testing::NiceMock<ConfigurationProviderMock>());
    ASSERT_NO_THROW(builder->configurationProvider(configurationProviderMock));

    EXPECT_CALL(*configurationProviderMock, getConfiguration).WillOnce([&]() {
        std::cout << "ConfigurationProvider: Providing configurations!" << std::endl;
        configurationProviderSuccess = true;
        return std::vector<wolkabout::ConfigurationItem>();
    });

    std::shared_ptr<wolkabout::Wolk> wolk = nullptr;
    EXPECT_NO_THROW(wolk = builder->build());

    wolk->handleActuatorSetCommand("KEY", "VALUE");

    const auto& configItem1 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2"}, "R1");
    const auto& configItem2 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2", "V3", "V4"}, "R2");
    const auto& configItem3 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2", "V3"}, "R3");
    const auto& configItem4 = wolkabout::ConfigurationItem(std::vector<std::string>{"V1"}, "R4");
    const auto& configSetCommand = wolkabout::ConfigurationSetCommand(
      std::vector<wolkabout::ConfigurationItem>{configItem1, configItem2, configItem3, configItem4});
    wolk->handleConfigurationSetCommand(configSetCommand);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(actuatorHandleSuccess);
    EXPECT_TRUE(actuatorStatusSuccess);
    EXPECT_TRUE(configurationHandlerSuccess);
    EXPECT_TRUE(configurationProviderSuccess);
}

TEST_F(WolkBuilderTests, OtherProperties)
{
    const auto& testDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    EXPECT_NO_THROW(builder->host("some_other_host"));
    // It doesn't really check for if the host is empty!
    EXPECT_NO_THROW(builder->host(""));

    std::shared_ptr<PersistenceMock> persistenceMock;
    persistenceMock.reset(new ::testing::NiceMock<PersistenceMock>());
    EXPECT_NO_THROW(builder->withPersistence(persistenceMock));

    std::unique_ptr<DataProtocolMock> dataProtocolMock(new ::testing::NiceMock<DataProtocolMock>());
    EXPECT_NO_THROW(builder->withDataProtocol(std::move(dataProtocolMock)));
}

TEST_F(WolkBuilderTests, NullChecks)
{
    auto testDevice1 = std::make_shared<wolkabout::Device>("", "", std::vector<std::string>());
    EXPECT_THROW(wolkabout::WolkBuilder(*testDevice1).build(), std::logic_error);
    testDevice1.reset();

    const auto& testDevice2 =
      std::make_shared<wolkabout::Device>("TEST_KEY", "", std::vector<std::string>{"A1", "A2", "A3"});
    std::shared_ptr<wolkabout::WolkBuilder> builder;

    EXPECT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice2));
    EXPECT_THROW(builder->build(), std::logic_error);

    bool notCalled = true;
    EXPECT_NO_THROW(builder->actuationHandler([&](const std::string& key, const std::string& value) {
        // This shouldn't be called.
        notCalled = false;
    }));

    EXPECT_THROW(builder->build(), std::logic_error);
    EXPECT_TRUE(notCalled);

    builder->m_actuatorStatusProviderLambda = nullptr;
    builder->m_actuationHandlerLambda = nullptr;

    const auto& actuationHandlerMock = std::make_shared<ActuationHandlerMock>();
    EXPECT_CALL(*actuationHandlerMock, handleActuation).Times(0);
    EXPECT_NO_THROW(builder->actuationHandler(actuationHandlerMock));

    EXPECT_THROW(builder->build(), std::logic_error);

    const auto& testDevice3 = std::make_shared<wolkabout::Device>("TEST_KEY", "", std::vector<std::string>());
    EXPECT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice3));

    ASSERT_NO_THROW(builder->configurationProvider([&]() { return std::vector<wolkabout::ConfigurationItem>(); }));

    EXPECT_THROW(builder->build(), std::logic_error);

    builder->m_configurationHandlerLambda = nullptr;
    builder->m_configurationProviderLambda = nullptr;

    ASSERT_NO_THROW(builder->configurationHandler([&](const std::vector<wolkabout::ConfigurationItem>& items) {}));

    EXPECT_THROW(builder->build(), std::logic_error);

    builder->m_configurationHandlerLambda = nullptr;
    builder->m_configurationProviderLambda = nullptr;

    std::shared_ptr<ConfigurationHandlerMock> configurationHandlerMock;
    configurationHandlerMock.reset(new ::testing::NiceMock<ConfigurationHandlerMock>());
    ASSERT_NO_THROW(builder->configurationHandler(configurationHandlerMock));

    EXPECT_THROW(builder->build(), std::logic_error);

    builder->m_configurationProvider.reset();
    builder->m_configurationHandler.reset();

    std::shared_ptr<ConfigurationProviderMock> configurationProviderMock;
    configurationProviderMock.reset(new ::testing::NiceMock<ConfigurationProviderMock>());
    ASSERT_NO_THROW(builder->configurationProvider(configurationProviderMock));

    EXPECT_THROW(builder->build(), std::logic_error);
}

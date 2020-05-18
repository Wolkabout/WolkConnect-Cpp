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
#include "service/DataService.h"
#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ConfigurationSetCommand.h"
#include "model/Message.h"
#undef private
#undef protected

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

#include "mocks/ConnectivityServiceMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/PersistenceMock.h"

class DataServiceTests : public ::testing::Test
{
public:
    void SetUp()
    {
        dataProtocolMock.reset(new ::testing::NiceMock<DataProtocolMock>());
        persistenceMock.reset(new ::testing::NiceMock<PersistenceMock>());
        connectivityServiceMock.reset(new ::testing::NiceMock<ConnectivityServiceMock>());
    }

    void TearDown()
    {
        dataProtocolMock.reset();
        persistenceMock.reset();
        connectivityServiceMock.reset();
    }

    std::unique_ptr<DataProtocolMock> dataProtocolMock;
    std::unique_ptr<PersistenceMock> persistenceMock;
    std::unique_ptr<ConnectivityServiceMock> connectivityServiceMock;
};

using namespace ::testing;

TEST_F(DataServiceTests, SimpleCtorTest)
{
    EXPECT_NO_THROW(wolkabout::DataService("TEST_DEVICE_KEY", *dataProtocolMock, *persistenceMock,
                                           *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr));
}

TEST_F(DataServiceTests, MessageChecksTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(
      dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
        key, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    auto message = std::make_shared<wolkabout::Message>("DATA", key);

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(key)).WillOnce(Return(""));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(key)).WillOnce(Return("SOMETHING_DIFFERENT"));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(key)).WillOnce(Return(key));
    EXPECT_NO_THROW(dataService->messageReceived(message));
}

TEST_F(DataServiceTests, ValidMessagesTests)
{
    const auto& deviceKey = "TEST_DEVICE_KEY";

    bool success[4];

    wolkabout::ActuatorGetHandler actuatorGetHandler = [&](const std::string& key) {
        std::cout << "ActuatorGetHandler: " << key << std::endl;
        success[0] = true;
    };

    wolkabout::ActuatorSetHandler actuatorSetHandler = [&](const std::string& key, const std::string& value) {
        std::cout << "ActuatorSetHandler: " << key << ", " << value << std::endl;
        success[1] = true;
    };

    wolkabout::ConfigurationGetHandler configurationGetHandler = [&]() {
        std::cout << "ConfigurationGetHandler: Called!" << std::endl;
        success[2] = true;
    };

    wolkabout::ConfigurationSetHandler configurationSetHandler = [&](const wolkabout::ConfigurationSetCommand& value) {
        std::cout << "ConfigurationSetHandler: Size : " << value.getValues().size() << std::endl;
        success[3] = true;
    };

    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
                      deviceKey, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, actuatorSetHandler,
                      actuatorGetHandler, configurationSetHandler, configurationGetHandler)));

    const auto& message = std::make_shared<wolkabout::Message>("DATA", deviceKey);

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorGetCommand).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorGetCommand)
      .WillOnce(
        Return(ByMove(std::unique_ptr<wolkabout::ActuatorGetCommand>(new wolkabout::ActuatorGetCommand("TEST_REF")))));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorSetCommand).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorSetCommand)
      .WillOnce(Return(ByMove(
        std::unique_ptr<wolkabout::ActuatorSetCommand>(new wolkabout::ActuatorSetCommand("TEST_REF", "TEST_VAL")))));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isConfigurationGetMessage).WillOnce(Return(true));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isConfigurationGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isConfigurationSetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeConfigurationSetCommand)
      .WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isConfigurationGetMessage).WillOnce(Return(false));
    EXPECT_CALL(*dataProtocolMock, isConfigurationSetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeConfigurationSetCommand)
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::ConfigurationSetCommand>(
        new wolkabout::ConfigurationSetCommand(std::vector<wolkabout::ConfigurationItem>())))));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    for (const auto& val : success)
    {
        EXPECT_TRUE(val);
    }
}

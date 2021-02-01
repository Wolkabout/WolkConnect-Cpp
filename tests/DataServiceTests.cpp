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
#include "service/data/DataService.h"
#undef private
#undef protected

#include "mocks/ConnectivityServiceMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/PersistenceMock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>

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

    EXPECT_EQ(&(*dataProtocolMock), &(dataService->getProtocol()));
}

TEST_F(DataServiceTests, ValidMessagesTests)
{
    const auto& deviceKey = "TEST_DEVICE_KEY";

    bool actuatorGetCalled = false;
    bool actuatorSetCalled = false;
    bool configurationGetCalled = false;
    bool configurationSetCalled = false;

    wolkabout::ActuatorGetHandler actuatorGetHandler = [&](const std::string& key) { actuatorGetCalled = true; };

    wolkabout::ActuatorSetHandler actuatorSetHandler = [&](const std::string& key, const std::string& value) {
        actuatorSetCalled = true;
    };

    wolkabout::ConfigurationGetHandler configurationGetHandler = [&]() { configurationGetCalled = true; };

    wolkabout::ConfigurationSetHandler configurationSetHandler = [&](const wolkabout::ConfigurationSetCommand& value) {
        configurationSetCalled = true;
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
    EXPECT_FALSE(actuatorGetCalled);

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorGetCommand)
      .WillOnce(
        Return(ByMove(std::unique_ptr<wolkabout::ActuatorGetCommand>(new wolkabout::ActuatorGetCommand("TEST_REF")))));
    EXPECT_NO_THROW(dataService->messageReceived(message));
    EXPECT_TRUE(actuatorGetCalled);

    EXPECT_CALL(*dataProtocolMock, isActuatorGetMessage).WillRepeatedly(Return(false));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorSetCommand).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->messageReceived(message));
    EXPECT_FALSE(actuatorSetCalled);

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillOnce(Return(true));
    EXPECT_CALL(*dataProtocolMock, makeActuatorSetCommand)
      .WillOnce(Return(ByMove(
        std::unique_ptr<wolkabout::ActuatorSetCommand>(new wolkabout::ActuatorSetCommand("TEST_REF", "TEST_VAL")))));
    EXPECT_NO_THROW(dataService->messageReceived(message));
    EXPECT_TRUE(actuatorSetCalled);

    EXPECT_CALL(*dataProtocolMock, isActuatorSetMessage).WillRepeatedly(Return(false));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, isConfigurationGetMessage).WillOnce(Return(true));
    EXPECT_NO_THROW(dataService->messageReceived(message));
    EXPECT_TRUE(configurationGetCalled);

    EXPECT_CALL(*dataProtocolMock, isConfigurationGetMessage).WillRepeatedly(Return(false));
    EXPECT_CALL(*dataProtocolMock, isConfigurationSetMessage).WillRepeatedly(Return(true));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, makeConfigurationSetCommand).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->messageReceived(message));
    EXPECT_FALSE(configurationSetCalled);

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(deviceKey)).WillOnce(Return(deviceKey));
    EXPECT_CALL(*dataProtocolMock, makeConfigurationSetCommand)
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::ConfigurationSetCommand>(
        new wolkabout::ConfigurationSetCommand(std::vector<wolkabout::ConfigurationItem>())))));
    EXPECT_NO_THROW(dataService->messageReceived(message));
    EXPECT_TRUE(configurationSetCalled);
}

TEST_F(DataServiceTests, PersistenceAddingTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(
      dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
        key, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    EXPECT_CALL(*persistenceMock, putSensorReading).WillOnce(Return(true));
    EXPECT_NO_THROW(dataService->addSensorReading("TEST_REF", "TEST_VALUE", 0));

    EXPECT_CALL(*persistenceMock, putSensorReading).WillOnce(Return(true));
    EXPECT_NO_THROW(
      dataService->addSensorReading("TEST_REF", std::vector<std::string>{"TEST_VALUE_1", "TEST_VALUE_2"}, 0));

    EXPECT_CALL(*persistenceMock, putAlarm).WillOnce(Return(true));
    EXPECT_NO_THROW(dataService->addAlarm("TEST_REF", true, 0));

    EXPECT_CALL(*persistenceMock, putActuatorStatus).WillOnce(Return(true));
    EXPECT_NO_THROW(dataService->addActuatorStatus("TEST_REF", "TEST_VALUE", wolkabout::ActuatorStatus::State::READY));

    EXPECT_CALL(*persistenceMock, putConfiguration).WillOnce(Return(true));
    EXPECT_NO_THROW(dataService->addConfiguration(std::vector<wolkabout::ConfigurationItem>{}));
}

TEST_F(DataServiceTests, PublishingSensorsTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(
      dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
        key, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    // Null checks and empty checks
    EXPECT_CALL(*persistenceMock, getSensorReadingsKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getSensorReadings)
      .WillOnce(Return(std::vector<std::shared_ptr<wolkabout::SensorReading>>()));
    EXPECT_NO_THROW(dataService->publishSensorReadings());

    const auto& readings = std::vector<std::shared_ptr<wolkabout::SensorReading>>{
      std::make_shared<wolkabout::SensorReading>("VALUE1", "REF1", 0)};

    EXPECT_CALL(*persistenceMock, getSensorReadingsKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getSensorReadings).WillOnce(Return(readings));
    EXPECT_CALL(*dataProtocolMock, makeMessage("TEST_DEVICE_KEY", readings)).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->publishSensorReadings());

    // Happy flow
    EXPECT_CALL(*persistenceMock, getSensorReadingsKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getSensorReadings)
      .Times(2)
      .WillOnce(Return(readings))
      .WillOnce(Return(std::vector<std::shared_ptr<wolkabout::SensorReading>>{}));
    EXPECT_CALL(*dataProtocolMock, makeMessage(key, readings))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>(new wolkabout::Message("HELLO", "HELLO")))));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    EXPECT_CALL(*persistenceMock, removeSensorReadings).WillOnce(Return());
    EXPECT_NO_THROW(dataService->publishSensorReadings());
}

TEST_F(DataServiceTests, PublishingAlarmsTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(
      dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
        key, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    // Null checks and empty checks
    EXPECT_CALL(*persistenceMock, getAlarmsKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getAlarms).WillOnce(Return(std::vector<std::shared_ptr<wolkabout::Alarm>>()));
    EXPECT_NO_THROW(dataService->publishAlarms());

    const auto& alarms =
      std::vector<std::shared_ptr<wolkabout::Alarm>>{std::make_shared<wolkabout::Alarm>("VALUE1", "REF1", 0)};

    EXPECT_CALL(*persistenceMock, getAlarmsKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getAlarms).WillOnce(Return(alarms));
    EXPECT_CALL(*dataProtocolMock, makeMessage("TEST_DEVICE_KEY", alarms)).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->publishAlarms());

    // Happy flow
    EXPECT_CALL(*persistenceMock, getAlarmsKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getAlarms)
      .Times(2)
      .WillOnce(Return(alarms))
      .WillOnce(Return(std::vector<std::shared_ptr<wolkabout::Alarm>>{}));
    EXPECT_CALL(*dataProtocolMock, makeMessage(key, alarms))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>(new wolkabout::Message("HELLO", "HELLO")))));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    EXPECT_CALL(*persistenceMock, removeAlarms).WillOnce(Return());
    EXPECT_NO_THROW(dataService->publishAlarms());
}

TEST_F(DataServiceTests, PublishingActuatorStatusTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(
      dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
        key, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    // Null checks and empty checks
    EXPECT_CALL(*persistenceMock, getActuatorStatusesKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getActuatorStatus).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->publishActuatorStatuses());

    const auto& actuatorStatus =
      std::make_shared<wolkabout::ActuatorStatus>("VALUE1", "REF1", wolkabout::ActuatorStatus::State::READY);

    EXPECT_CALL(*persistenceMock, getActuatorStatusesKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getActuatorStatus).WillOnce(Return(actuatorStatus));
    EXPECT_CALL(*dataProtocolMock,
                makeMessage(key, std::vector<std::shared_ptr<wolkabout::ActuatorStatus>>{actuatorStatus}))
      .WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->publishActuatorStatuses());

    // Happy flow
    EXPECT_CALL(*persistenceMock, getActuatorStatusesKeys).WillOnce(Return(std::vector<std::string>{"KEY1"}));
    EXPECT_CALL(*persistenceMock, getActuatorStatus).WillOnce(Return(actuatorStatus));
    EXPECT_CALL(*dataProtocolMock,
                makeMessage(key, std::vector<std::shared_ptr<wolkabout::ActuatorStatus>>{actuatorStatus}))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>(new wolkabout::Message("HELLO", "HELLO")))));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    EXPECT_CALL(*persistenceMock, removeActuatorStatus).WillOnce(Return());
    EXPECT_NO_THROW(dataService->publishActuatorStatuses());
}

TEST_F(DataServiceTests, PublishingConfigurationsTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(
      dataService = std::unique_ptr<wolkabout::DataService>(new wolkabout::DataService(
        key, *dataProtocolMock, *persistenceMock, *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    // Null checks and empty checks
    EXPECT_CALL(*persistenceMock, getConfiguration).WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->publishConfiguration());

    const auto& configurationItems =
      std::make_shared<std::vector<wolkabout::ConfigurationItem>>(std::vector<wolkabout::ConfigurationItem>{
        wolkabout::ConfigurationItem(std::vector<std::string>{"VALUE1", "VALUE2"}, "REF")});

    EXPECT_CALL(*persistenceMock, getConfiguration).WillOnce(Return(configurationItems));
    EXPECT_CALL(*dataProtocolMock, makeMessage(key, A<const std::vector<wolkabout::ConfigurationItem>&>()))
      .WillOnce(Return(ByMove(nullptr)));
    EXPECT_NO_THROW(dataService->publishConfiguration());

    // Happy flow
    EXPECT_CALL(*persistenceMock, getConfiguration).WillOnce(Return(configurationItems));
    EXPECT_CALL(*dataProtocolMock, makeMessage(key, A<const std::vector<wolkabout::ConfigurationItem>&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>(new wolkabout::Message("HELLO", "HELLO")))));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    EXPECT_CALL(*persistenceMock, removeConfiguration).WillOnce(Return());
    EXPECT_NO_THROW(dataService->publishConfiguration());
}

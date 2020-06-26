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
#include "Wolk.h"
#undef private
#undef protected

#include "mocks/ConnectivityServiceMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/DataServiceMock.h"
#include "mocks/InboundMessageHandlerMock.h"
#include "mocks/KeepAliveServiceMock.h"
#include "mocks/PersistenceMock.h"
#include "mocks/StatusProtocolMock.h"

#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ConfigurationSetCommand.h"
#include "model/DeviceStatus.h"
#include "model/Device.h"
#include "model/Message.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class WolkTests : public ::testing::Test
{
public:
    std::shared_ptr<wolkabout::Device> noActuatorsDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>());
    std::shared_ptr<wolkabout::Device> device =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    std::shared_ptr<wolkabout::WolkBuilder> builder;
};

TEST_F(WolkTests, Notifies)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
    ASSERT_NO_THROW(builder->withoutKeepAlive());
    const auto& wolk = builder->build();

    auto statusProtocolMock = std::unique_ptr<StatusProtocolMock>(new ::testing::NiceMock<StatusProtocolMock>());
    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());

    auto keepAliveServiceMock = std::unique_ptr<KeepAliveServiceMock>(new ::testing::NiceMock<KeepAliveServiceMock>(
      noActuatorsDevice->getKey(), *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(60)));
    wolk->m_keepAliveService = std::move(keepAliveServiceMock);

    EXPECT_CALL(dynamic_cast<KeepAliveServiceMock&>(*(wolk->m_keepAliveService)), connected).Times(1);
    EXPECT_CALL(dynamic_cast<KeepAliveServiceMock&>(*(wolk->m_keepAliveService)), disconnected).Times(1);

    EXPECT_NO_FATAL_FAILURE(wolk->notifyConnected());
    EXPECT_NO_FATAL_FAILURE(wolk->notifyDisonnected());
}

TEST_F(WolkTests, ConnectTest)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
    ASSERT_NO_THROW(builder->withoutKeepAlive());
    const auto& wolk = builder->build();

    auto statusProtocolMock = std::unique_ptr<StatusProtocolMock>(new ::testing::NiceMock<StatusProtocolMock>());
    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());
    auto keepAliveServiceMock = std::unique_ptr<KeepAliveServiceMock>(new ::testing::NiceMock<KeepAliveServiceMock>(
      noActuatorsDevice->getKey(), *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(60)));

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new ::testing::NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new ::testing::NiceMock<PersistenceMock>());
    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new ::testing::NiceMock<DataServiceMock>(
      noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));

    wolk->m_dataService = std::move(dataServiceMock);
    wolk->m_connectivityService = std::move(connectivityServiceMock);

    EXPECT_CALL(dynamic_cast<ConnectivityServiceMock&>(*(wolk->m_connectivityService)), connect)
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

    EXPECT_NO_FATAL_FAILURE(wolk->connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

TEST_F(WolkTests, DisconnectTest)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
    ASSERT_NO_THROW(builder->withoutKeepAlive());
    const auto& wolk = builder->build();

    auto statusProtocolMock = std::unique_ptr<StatusProtocolMock>(new ::testing::NiceMock<StatusProtocolMock>());
    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());
    auto keepAliveServiceMock = std::unique_ptr<KeepAliveServiceMock>(new ::testing::NiceMock<KeepAliveServiceMock>(
      noActuatorsDevice->getKey(), *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(60)));

    wolk->m_keepAliveService = std::move(keepAliveServiceMock);
    wolk->m_connectivityService = std::move(connectivityServiceMock);

    EXPECT_NO_FATAL_FAILURE(wolk->disconnect());
}

TEST_F(WolkTests, AddingSensors)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
    ASSERT_NO_THROW(builder->withoutKeepAlive());
    const auto& wolk = builder->build();

    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());
    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new ::testing::NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new ::testing::NiceMock<PersistenceMock>());
    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new ::testing::NiceMock<DataServiceMock>(
      noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));

    wolk->m_dataService = std::move(dataServiceMock);

    EXPECT_CALL(*persistenceMock, putSensorReading).Times(2).WillRepeatedly(testing::Return(true));

    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF1", 100));
    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF2", std::vector<int>{1, 2, 3}));
    // Empty test
    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF3", std::vector<int>{}));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(WolkTests, AddAlarms)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
    ASSERT_NO_THROW(builder->withoutKeepAlive());
    const auto& wolk = builder->build();

    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());
    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new ::testing::NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new ::testing::NiceMock<PersistenceMock>());
    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new ::testing::NiceMock<DataServiceMock>(
      noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));

    wolk->m_dataService = std::move(dataServiceMock);

    EXPECT_NO_FATAL_FAILURE(wolk->addAlarm("TEST_ALARM_REF1", true));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(WolkTests, HandleActuatorCommands)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*device);

    auto actuationHandler = [&](const std::string& ref, const std::string& value) {
        std::cout << "ActuatorHandler: " << ref << ", " << value << std::endl;
    };

    auto actuatorStatusProvider = [&](const std::string& ref) {
        std::cout << "ActuatorStatusProvider: " << ref << std::endl;
        return wolkabout::ActuatorStatus();
    };

    builder->actuationHandler(actuationHandler);
    builder->actuatorStatusProvider(actuatorStatusProvider);
    ASSERT_NO_THROW(builder->withoutKeepAlive());

    const auto& wolk = builder->build();

    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());
    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new ::testing::NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new ::testing::NiceMock<PersistenceMock>());
    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new ::testing::NiceMock<DataServiceMock>(
      noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));

    EXPECT_CALL(*persistenceMock, putActuatorStatus).Times(2).WillRepeatedly(testing::Return(true));

    wolk->m_dataService = std::move(dataServiceMock);

    EXPECT_NO_FATAL_FAILURE(wolk->handleActuatorGetCommand("TEST_REF1"));
    EXPECT_NO_FATAL_FAILURE(wolk->handleActuatorSetCommand("TEST_REF1", "VALUE1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(WolkTests, HandleConfigurationCommands)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);

    auto configurationHandler = [&](const std::vector<wolkabout::ConfigurationItem>& items) {
        std::cout << "ConfigurationHandler: " << items.size() << std::endl;
    };

    auto configurationProvider = [&]() -> std::vector<wolkabout::ConfigurationItem> {
        std::cout << "ConfigurationProvider: Invoked!" << std::endl;
        return std::vector<wolkabout::ConfigurationItem>();
    };

    builder->configurationHandler(configurationHandler);
    builder->configurationProvider(configurationProvider);
    ASSERT_NO_THROW(builder->withoutKeepAlive());

    const auto& wolk = builder->build();

    auto connectivityServiceMock =
      std::unique_ptr<ConnectivityServiceMock>(new ::testing::NiceMock<ConnectivityServiceMock>());
    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new ::testing::NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new ::testing::NiceMock<PersistenceMock>());
    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new ::testing::NiceMock<DataServiceMock>(
      noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));

    EXPECT_CALL(*persistenceMock, putConfiguration).Times(2).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*persistenceMock, getConfiguration).Times(2).WillRepeatedly(testing::Return(nullptr));

    wolk->m_dataService = std::move(dataServiceMock);

    EXPECT_NO_FATAL_FAILURE(wolk->handleConfigurationGetCommand());
    EXPECT_NO_FATAL_FAILURE(wolk->handleConfigurationSetCommand(
      wolkabout::ConfigurationSetCommand(std::vector<wolkabout::ConfigurationItem>())));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(WolkTests, ConnectivityFacade)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
    ASSERT_NO_THROW(builder->withoutKeepAlive());
    const auto& wolk = builder->build();

    const auto& inboundMessageHandlerMock =
      std::unique_ptr<InboundMessageHandlerMock>(new ::testing::NiceMock<InboundMessageHandlerMock>());

    bool channel1Invoked = false, channel2Invoked = false, connectionLostInvoked = false, channelsRequested = false;

//    This part doesn't work, since the class contains a reference, and not a pointer.
//    EXPECT_CALL(*inboundMessageHandlerMock, messageReceived)
//      .WillOnce([&](const std::string& channel, const std::string& message) {
//          if (channel == "CHANNEL1")
//              channel1Invoked = true;
//          else if (channel == "CHANNEL2")
//              channel2Invoked = true;
//          std::cout << "InboundMessageHandlerMock: " << channel << ", " << message << std::endl;
//      });
//
//    EXPECT_CALL(*inboundMessageHandlerMock, getChannels).WillOnce([&]() {
//        channelsRequested = true;
//        return std::vector<std::string>{"CHANNEL1", "CHANNEL2"};
//    });
//
//    wolk->m_connectivityManager->m_messageHandler =
//      static_cast<wolkabout::InboundMessageHandler&>(*inboundMessageHandlerMock);

    wolk->m_connectivityManager->m_connectionLostHandler = [&]() {
        connectionLostInvoked = true;
        std::cout << "ConnectionLostHandler: Invoked!" << std::endl;
    };

//    EXPECT_NO_FATAL_FAILURE(wolk->m_connectivityManager->messageReceived("CHANNEL1", "MESSAGE1"));
//    EXPECT_NO_FATAL_FAILURE(wolk->m_connectivityManager->messageReceived("CHANNEL2", "MESSAGE2"));
//
//    EXPECT_EQ(wolk->m_connectivityManager->getChannels().size(), 2);

    EXPECT_NO_FATAL_FAILURE(wolk->m_connectivityManager->connectionLost());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

//    EXPECT_TRUE(channel1Invoked);
//    EXPECT_TRUE(channel2Invoked);
    EXPECT_TRUE(connectionLostInvoked);
//    EXPECT_TRUE(channelsRequested);
}

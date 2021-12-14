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
#include "wolk/Wolk.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/ConnectivityServiceMock.h"
#include "tests/mocks/DataProtocolMock.h"
#include "tests/mocks/PersistenceMock.h"
#include "tests/mocks/DataServiceMock.h"
#include "tests/mocks/FileManagementServiceMock.h"
#include "tests/mocks/FileManagementProtocolMock.h"
#include "tests/mocks/InboundMessageHandlerMock.h"

#include <gtest/gtest.h>

using namespace wolkabout;
using namespace ::testing;

class WolkTests : public Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    const std::string TAG = "WolkTests";

    std::shared_ptr<wolkabout::Device> device =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", OutboundDataMode::PUSH);

    std::shared_ptr<wolkabout::WolkBuilder> builder;

    std::mutex mutex;
    std::condition_variable cv;
    int eventsToWait = 0;

    void onEvent()
    {
        std::lock_guard<std::mutex> lock{mutex};
        eventsToWait--;
        cv.notify_one();
    }

    void waitEvents(int eventCount = 1, std::chrono::milliseconds period = std::chrono::milliseconds{500})
    {
        std::unique_lock<std::mutex> lock{mutex};
        eventsToWait = eventCount;
        EXPECT_TRUE(cv.wait_for(lock, period, [this] { return eventsToWait <= 0; }));
    }
};

TEST_F(WolkTests, Notifies)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*device);
    const auto& wolk = builder->build();

    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    EXPECT_NO_FATAL_FAILURE(wolk->notifyConnected());
    EXPECT_NO_FATAL_FAILURE(wolk->notifyDisonnected());
}

TEST_F(WolkTests, ConnectTest)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*device);
    const auto& wolk = builder->build();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    auto feedHandler = [&](const std::map<std::uint64_t, std::vector<Reading>>&) {};
    auto parameterHandler = [&](const std::vector<Parameter>&) {};

    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
      device->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock, feedHandler, parameterHandler));

    wolk->m_dataService = std::move(dataServiceMock);
    wolk->m_connectivityService = std::move(connectivityServiceMock);

    EXPECT_CALL(dynamic_cast<ConnectivityServiceMock&>(*(wolk->m_connectivityService)), connect)
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(this, &WolkTests::onEvent), testing::Return(true)));

    EXPECT_NO_FATAL_FAILURE(wolk->connect());

    waitEvents(1);
}

TEST_F(WolkTests, WhenConnected_PublishFileList)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*device);
    const auto& wolk = builder->build();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    auto feedHandler = [&](const std::map<std::uint64_t, std::vector<Reading>>&) {};
    auto parameterHandler = [&](const std::vector<Parameter>&) {};

    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
      device->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock, feedHandler, parameterHandler));

    auto fileManagementProtocolMock =
      std::unique_ptr<FileManagementProtocolMock>(new NiceMock<FileManagementProtocolMock>);
    auto fileManagementServiceMock = std::make_shared<FileManagementServiceMock>(
      device->getKey(), *connectivityServiceMock, *dataServiceMock, *fileManagementProtocolMock, "./");

    wolk->m_dataService = std::move(dataServiceMock);
    wolk->m_connectivityService = std::move(connectivityServiceMock);
    wolk->m_fileManagementService = std::move(fileManagementServiceMock);

    ON_CALL(dynamic_cast<ConnectivityServiceMock&>(*(wolk->m_connectivityService)), connect)
      .WillByDefault(testing::Return(true));

    EXPECT_CALL(dynamic_cast<FileManagementServiceMock&>(*(wolk->m_fileManagementService)), reportAllPresentFiles)
      .WillOnce(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));

    EXPECT_NO_FATAL_FAILURE(wolk->connect());

    waitEvents(1);
}

TEST_F(WolkTests, DisconnectTest)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*device);
    const auto& wolk = builder->build();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    wolk->m_connectivityService = std::move(connectivityServiceMock);

    EXPECT_NO_FATAL_FAILURE(wolk->disconnect());
}

// TEST_F(WolkTests, AddingSensors)
//{
//     builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
//     ASSERT_NO_THROW(builder->withoutKeepAlive());
//     const auto& wolk = builder->build();
//
//     auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());
//     auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
//     auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
//     auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
//       noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));
//
//     wolk->m_dataService = std::move(dataServiceMock);
//
//     EXPECT_CALL(
//       dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)),
//       addSensorReading(Matcher<const std::string&>(_), Matcher<const std::string&>(_), Matcher<unsigned long
//       long>(_))) .Times(1) .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));
//
//     EXPECT_CALL(dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)),
//                 addSensorReading(Matcher<const std::string&>(_), Matcher<const std::vector<std::string>&>(_),
//                                  Matcher<unsigned long long>(_)))
//       .Times(1)
//       .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));
//
//     EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF1", 100));
//     EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF2", std::vector<int>{1, 2, 3}));
//     // Empty test
//     EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF3", std::vector<int>{}));
//
//     waitEvents(2);
// }
//
// TEST_F(WolkTests, AddAlarms)
//{
//     builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
//     ASSERT_NO_THROW(builder->withoutKeepAlive());
//     const auto& wolk = builder->build();
//
//     auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());
//     auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
//     auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
//     auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
//       noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));
//
//     wolk->m_dataService = std::move(dataServiceMock);
//
//     EXPECT_CALL(dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)), addAlarm)
//       .Times(1)
//       .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));
//
//     EXPECT_NO_FATAL_FAILURE(wolk->addAlarm("TEST_ALARM_REF1", true));
//
//     waitEvents(1);
// }
//
// TEST_F(WolkTests, HandleActuatorCommands)
//{
//     builder = std::make_shared<wolkabout::WolkBuilder>(*device);
//
//     auto actuationHandler = [&](const std::string& ref, const std::string& value)
//     { std::cout << "ActuatorHandler: " << ref << ", " << value << std::endl; };
//
//     auto actuatorStatusProvider = [&](const std::string& ref)
//     {
//         std::cout << "ActuatorStatusProvider: " << ref << std::endl;
//         return wolkabout::ActuatorStatus();
//     };
//
//     builder->actuationHandler(actuationHandler);
//     builder->actuatorStatusProvider(actuatorStatusProvider);
//     ASSERT_NO_THROW(builder->withoutKeepAlive());
//
//     const auto& wolk = builder->build();
//
//     auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());
//     auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
//     auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
//     auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
//       noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));
//
//     wolk->m_dataService = std::move(dataServiceMock);
//
//     EXPECT_CALL(dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)), addActuatorStatus)
//       .Times(2)
//       .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));
//
//     EXPECT_NO_FATAL_FAILURE(wolk->handleActuatorGetCommand("TEST_REF1"));
//     EXPECT_NO_FATAL_FAILURE(wolk->handleActuatorSetCommand("TEST_REF1", "VALUE1"));
//
//     waitEvents(2);
// }
//
// TEST_F(WolkTests, HandleConfigurationCommands)
//{
//     builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
//
//     auto configurationHandler = [&](const std::vector<wolkabout::ConfigurationItem>& items)
//     { std::cout << "ConfigurationHandler: " << items.size() << std::endl; };
//
//     auto configurationProvider = [&]() -> std::vector<wolkabout::ConfigurationItem>
//     {
//         std::cout << "ConfigurationProvider: Invoked!" << std::endl;
//         return std::vector<wolkabout::ConfigurationItem>();
//     };
//
//     builder->configurationHandler(configurationHandler);
//     builder->configurationProvider(configurationProvider);
//     ASSERT_NO_THROW(builder->withoutKeepAlive());
//
//     const auto& wolk = builder->build();
//
//     auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());
//     auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
//     auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
//     auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
//       noActuatorsDevice->getKey(), *dataProtocolMock, *persistenceMock, *connectivityServiceMock));
//
//     wolk->m_dataService = std::move(dataServiceMock);
//
//     EXPECT_CALL(dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)), addConfiguration)
//       .Times(2)
//       .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));
//
//     EXPECT_NO_FATAL_FAILURE(wolk->handleConfigurationGetCommand());
//     EXPECT_NO_FATAL_FAILURE(wolk->handleConfigurationSetCommand(
//       wolkabout::ConfigurationSetCommand(std::vector<wolkabout::ConfigurationItem>())));
//
//     waitEvents(2);
// }

TEST_F(WolkTests, ConnectivityFacade)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*device);
    const auto& wolk = builder->build();

    const auto& inboundMessageHandlerMock =
      std::unique_ptr<InboundMessageHandlerMock>(new NiceMock<InboundMessageHandlerMock>());

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

    wolk->m_connectivityManager->m_connectionLostHandler = [&]()
    {
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

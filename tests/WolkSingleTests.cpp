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
#include "wolk/WolkBuilder.h"
#include "wolk/WolkSingle.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/ConnectivityServiceMock.h"
#include "tests/mocks/DataProtocolMock.h"
#include "tests/mocks/DataServiceMock.h"
#include "tests/mocks/ErrorProtocolMock.h"
#include "tests/mocks/ErrorServiceMock.h"
#include "tests/mocks/FileManagementProtocolMock.h"
#include "tests/mocks/FileManagementServiceMock.h"
#include "tests/mocks/FirmwareInstallerMock.h"
#include "tests/mocks/FirmwareParametersListenerMock.h"
#include "tests/mocks/FirmwareUpdateProtocolMock.h"
#include "tests/mocks/FirmwareUpdateServiceMock.h"
#include "tests/mocks/OutboundMessageHandlerMock.h"
#include "tests/mocks/OutboundRetryMessageHandlerMock.h"
#include "tests/mocks/PersistenceMock.h"

#include <gtest/gtest.h>

using namespace wolkabout;
using namespace wolkabout::connect;
using namespace ::testing;

class WolkSingleTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    void SetUp() override
    {
        service = std::unique_ptr<WolkSingle>{new WolkSingle{device}};
        service->m_connectivityService =
          std::unique_ptr<ConnectivityServiceMock>{new NiceMock<ConnectivityServiceMock>};
        service->m_outboundMessageHandler = new OutboundMessageHandlerMock();
        service->m_outboundRetryMessageHandler =
          std::make_shared<OutboundRetryMessageHandlerMock>(*service->m_outboundMessageHandler);
        service->m_dataService = std::unique_ptr<DataServiceMock>{new NiceMock<DataServiceMock>{
          dataProtocolMock, persistenceMock, *service->m_connectivityService, *service->m_outboundRetryMessageHandler,
          [](std::string, std::map<std::uint64_t, std::vector<Reading>>) {}, [](std::string, std::vector<Parameter>) {},
          [](std::string, std::vector<std::string>, std::vector<std::string>) {}}};
        service->m_errorService =
          std::unique_ptr<ErrorServiceMock>{new ErrorServiceMock{errorProtocolMock, std::chrono::seconds{10}}};
    }

    void TearDown() override { delete service->m_outboundMessageHandler; }

    ConnectivityServiceMock& GetConnectivityServiceReference() const
    {
        return dynamic_cast<ConnectivityServiceMock&>(*service->m_connectivityService);
    }
    DataServiceMock& GetDataServiceReference() const { return dynamic_cast<DataServiceMock&>(*service->m_dataService); }
    ErrorServiceMock& GetErrorServiceReference() const
    {
        return dynamic_cast<ErrorServiceMock&>(*service->m_errorService);
    }

    void Notify() { conditionVariable.notify_one(); }

    void Await(std::chrono::milliseconds time = std::chrono::milliseconds{100})
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, time);
    }

    const Device device{"TestDevice", "TestPassword", OutboundDataMode::PUSH};

    std::mutex mutex;
    std::condition_variable conditionVariable;

    std::unique_ptr<WolkSingle> service;

    DataProtocolMock dataProtocolMock;

    PersistenceMock persistenceMock;

    ErrorProtocolMock errorProtocolMock;

    FileManagementProtocolMock fileManagementProtocolMock;

    FirmwareUpdateProtocolMock firmwareUpdateProtocolMock;
};

TEST_F(WolkSingleTests, NewBuilder)
{
    ASSERT_NO_FATAL_FAILURE(WolkSingle::newBuilder(device).buildWolkSingle());
}

TEST_F(WolkSingleTests, GetType)
{
    EXPECT_EQ(service->getType(), WolkInterfaceType::SingleDevice);
}

TEST_F(WolkSingleTests, AddReadingStringValue)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReading(device.getKey(), _, A<const std::string&>(), _))
      .WillOnce([&](const std::string&, const std::string&, const std::string&, std::uint64_t) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReading("T", "TestValue"));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AddReadingStringValues)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReading(device.getKey(), _, A<const std::vector<std::string>&>(), _))
      .WillOnce([&](const std::string&, const std::string&, const std::vector<std::string>&, std::uint64_t) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReading("T", std::vector<std::string>{"TestValue"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AddReadingSingleReading)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReading(device.getKey(), A<const Reading&>()))
      .WillOnce([&](const std::string&, const Reading&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReading(Reading{"T", std::string{"TestValue"}}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AddReadingMultipleReadings)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReadings(device.getKey(), A<const std::vector<Reading>&>()))
      .WillOnce([&](const std::string&, const std::vector<Reading>&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReadings({Reading{"T", std::string{"TestValue"}}}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AddFeed)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), registerFeed(device.getKey(), _))
      .WillOnce([&](const std::string&, const Feed&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->registerFeed(Feed{"TestFeed", "TF", FeedType::IN_OUT, "NUMERIC"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AddFeeds)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), registerFeeds(device.getKey(), _))
      .WillOnce([&](const std::string&, const std::vector<Feed>&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->registerFeeds({Feed{"TestFeed", "TF", FeedType::IN_OUT, "NUMERIC"}}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, RemoveFeed)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), removeFeed(device.getKey(), _))
      .WillOnce([&](const std::string&, const std::string&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->removeFeed("TestFeed"));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, RemoveFeeds)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), removeFeeds(device.getKey(), _))
      .WillOnce([&](const std::string&, const std::vector<std::string>&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->removeFeeds({"TestFeed"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, PullFeedValues)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), pullFeedValues(device.getKey())).WillOnce([&](const std::string&) {
        called = true;
        Notify();
    });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->pullFeedValues());
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, PullParameters)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), pullParameters(device.getKey())).WillOnce([&](const std::string&) {
        called = true;
        Notify();
    });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->pullParameters());
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, SynchronizeParameters)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), synchronizeParameters(device.getKey(), _, _))
      .WillOnce(
        [&](const std::string&, const std::vector<ParameterName>&, std::function<void(std::vector<Parameter>)>) {
            called = true;
            Notify();
            return true;
        });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->synchronizeParameters({}, {}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AddAttribute)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addAttribute(device.getKey(), _))
      .WillOnce([&](const std::string&, const Attribute&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addAttribute(Attribute{"TestAttribute", DataType::STRING, "TestValue"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, UpdateParameter)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), updateParameter(device.getKey(), _))
      .WillOnce([&](const std::string&, const Parameter&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->updateParameter(Parameter{ParameterName::EXTERNAL_ID, "TestValue"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, AwaitError)
{
    EXPECT_CALL(GetErrorServiceReference(), obtainOrAwaitMessageForDevice).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->awaitError());
}

TEST_F(WolkSingleTests, NotifyConnectedFileManagement)
{
    auto fileManagementService = std::unique_ptr<FileManagementServiceMock>{new NiceMock<FileManagementServiceMock>{
      *service->m_connectivityService, *service->m_dataService, fileManagementProtocolMock, "./"}};
    EXPECT_CALL(*fileManagementService, reportPresentFiles).Times(1);
    service->m_fileManagementService = std::move(fileManagementService);
    ASSERT_NO_FATAL_FAILURE(service->notifyConnected());
}

TEST_F(WolkSingleTests, NotifyConnectedFirmwareUpdateInstaller)
{
    auto firmwareUpdateService = std::unique_ptr<FirmwareUpdateServiceMock>{new NiceMock<FirmwareUpdateServiceMock>{
      *service->m_connectivityService, *service->m_dataService,
      std::unique_ptr<FirmwareInstallerMock>{new NiceMock<FirmwareInstallerMock>()}, firmwareUpdateProtocolMock}};
    firmwareUpdateService->m_queue.push(std::make_shared<wolkabout::Message>("", ""));
    EXPECT_CALL(GetConnectivityServiceReference(), publish).Times(1);
    service->m_firmwareUpdateService = std::move(firmwareUpdateService);
    ASSERT_NO_FATAL_FAILURE(service->notifyConnected());
}

TEST_F(WolkSingleTests, NotifyConnectedFirmwareUpdateParameterListener)
{
    auto firmwareUpdateService = std::unique_ptr<FirmwareUpdateServiceMock>{new NiceMock<FirmwareUpdateServiceMock>{
      *service->m_connectivityService, *service->m_dataService,
      std::unique_ptr<FirmwareParametersListenerMock>{new NiceMock<FirmwareParametersListenerMock>()},
      firmwareUpdateProtocolMock}};
    EXPECT_CALL(*firmwareUpdateService, obtainParametersAndAnnounce).Times(1);
    service->m_firmwareUpdateService = std::move(firmwareUpdateService);
    ASSERT_NO_FATAL_FAILURE(service->notifyConnected());
}

TEST_F(WolkSingleTests, ConnectionToggles)
{
    std::atomic_bool called;
    ASSERT_NO_FATAL_FAILURE(service->setConnectionStatusListener([&](bool status) {
        called = status;
        Notify();
    }));
    EXPECT_CALL(GetConnectivityServiceReference(), connect).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->connect());
    if (!called)
        Await();
    ASSERT_TRUE(called);
    ASSERT_TRUE(service->isConnected());
    EXPECT_CALL(GetConnectivityServiceReference(), disconnect).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->disconnect());
    if (called)
        Await();
    EXPECT_FALSE(called);
    EXPECT_FALSE(service->isConnected());
}

TEST_F(WolkSingleTests, FeedUpdateHandlerLambda)
{
    std::atomic_bool called{false};
    service->m_feedUpdateHandler.reset();
    service->m_feedUpdateHandlerLambda = [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) {
        called = true;
        Notify();
    };

    ASSERT_NO_FATAL_FAILURE(service->handleFeedUpdateCommand("", {}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkSingleTests, ParameterUpdateHandlerLambda)
{
    std::atomic_bool called{false};
    service->m_parameterHandler.reset();
    service->m_parameterLambda = [&](const std::string&, const std::vector<Parameter>&) {
        called = true;
        Notify();
    };

    ASSERT_NO_FATAL_FAILURE(service->handleParameterCommand("", {}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

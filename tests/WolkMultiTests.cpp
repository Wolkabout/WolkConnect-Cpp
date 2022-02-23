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
#include "wolk/WolkMulti.h"
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
#include "tests/mocks/PersistenceMock.h"
#include "tests/mocks/RegistrationProtocolMock.h"
#include "tests/mocks/RegistrationServiceMock.h"

#include <gtest/gtest.h>

using namespace wolkabout;
using namespace wolkabout::connect;
using namespace ::testing;

class WolkMultiTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    void SetUp() override
    {
        service = std::unique_ptr<WolkMulti>{new WolkMulti{devices}};
        service->m_connectivityService =
          std::unique_ptr<ConnectivityServiceMock>{new NiceMock<ConnectivityServiceMock>};
        service->m_inboundMessageHandler = std::make_shared<InboundPlatformMessageHandler>(
          std::vector<std::string>{devices[0].getKey(), devices[1].getKey()});
        service->m_dataService = std::unique_ptr<DataServiceMock>{
          new NiceMock<DataServiceMock>{dataProtocolMock, persistenceMock, *service->m_connectivityService,
                                        [](std::string, std::map<std::uint64_t, std::vector<Reading>>) {},
                                        [](std::string, std::vector<Parameter>) {}}};
        service->m_errorService = std::unique_ptr<ErrorServiceMock>{
          new NiceMock<ErrorServiceMock>{errorProtocolMock, std::chrono::seconds{10}}};
        service->m_registrationService = std::unique_ptr<RegistrationServiceMock>{new NiceMock<RegistrationServiceMock>{
          registrationProtocolMock, *service->m_connectivityService, *service->m_errorService}};
    }

    void SetUpFileManagement()
    {
        service->m_fileManagementService =
          std::unique_ptr<FileManagementServiceMock>{new NiceMock<FileManagementServiceMock>{
            *service->m_connectivityService, *service->m_dataService, fileManagementProtocolMock, "./"}};
    }

    void SetUpFirmwareUpdateInstaller()
    {
        service->m_firmwareUpdateService =
          std::unique_ptr<FirmwareUpdateServiceMock>{new NiceMock<FirmwareUpdateServiceMock>{
            *service->m_connectivityService, *service->m_dataService,
            std::unique_ptr<FirmwareInstallerMock>{new NiceMock<FirmwareInstallerMock>()}, firmwareUpdateProtocolMock}};
    }

    void SetUpFirmwareUpdateParameterListener()
    {
        service->m_firmwareUpdateService =
          std::unique_ptr<FirmwareUpdateServiceMock>{new NiceMock<FirmwareUpdateServiceMock>{
            *service->m_connectivityService, *service->m_dataService,
            std::unique_ptr<FirmwareParametersListenerMock>{new NiceMock<FirmwareParametersListenerMock>()},
            firmwareUpdateProtocolMock}};
    }

    ConnectivityServiceMock& GetConnectivityServiceReference() const
    {
        return dynamic_cast<ConnectivityServiceMock&>(*service->m_connectivityService);
    }
    DataServiceMock& GetDataServiceReference() const { return dynamic_cast<DataServiceMock&>(*service->m_dataService); }
    ErrorServiceMock& GetErrorServiceReference() const
    {
        return dynamic_cast<ErrorServiceMock&>(*service->m_errorService);
    }
    RegistrationServiceMock& GetRegistrationServiceReference() const
    {
        return dynamic_cast<RegistrationServiceMock&>(*service->m_registrationService);
    };
    FileManagementServiceMock& GetFileManagementServiceReference() const
    {
        return dynamic_cast<FileManagementServiceMock&>(*service->m_fileManagementService);
    };
    FirmwareUpdateServiceMock& GetFirmwareUpdateServiceReference() const
    {
        return dynamic_cast<FirmwareUpdateServiceMock&>(*service->m_firmwareUpdateService);
    };

    void Notify() { conditionVariable.notify_one(); }

    void Await(std::chrono::milliseconds time = std::chrono::milliseconds{100})
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, time);
    }

    const std::vector<Device> devices{{"TestDevice1", "", OutboundDataMode::PUSH},
                                      {"TestDevice2", "", OutboundDataMode::PUSH}};

    std::mutex mutex;
    std::condition_variable conditionVariable;

    std::unique_ptr<WolkMulti> service;

    DataProtocolMock dataProtocolMock;

    PersistenceMock persistenceMock;

    ErrorProtocolMock errorProtocolMock;

    RegistrationProtocolMock registrationProtocolMock;

    FileManagementProtocolMock fileManagementProtocolMock;

    FirmwareUpdateProtocolMock firmwareUpdateProtocolMock;
};

TEST_F(WolkMultiTests, NewBuilder)
{
    ASSERT_NO_FATAL_FAILURE(WolkMulti::newBuilder(devices).buildWolkMulti());
}

TEST_F(WolkMultiTests, GetType)
{
    EXPECT_EQ(service->getType(), WolkInterfaceType::MultiDevice);
}

TEST_F(WolkMultiTests, IsDeviceInList)
{
    EXPECT_TRUE(service->isDeviceInList(devices[0]));
    EXPECT_TRUE(service->isDeviceInList(devices[0].getKey()));
    EXPECT_TRUE(service->isDeviceInList(devices[1]));
    EXPECT_TRUE(service->isDeviceInList(devices[1].getKey()));
    EXPECT_FALSE(service->isDeviceInList("TestDevice"));
}

TEST_F(WolkMultiTests, AddReadingStringValue)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReading(devices.front().getKey(), _, A<const std::string&>(), _))
      .WillOnce([&](const std::string&, const std::string&, const std::string&, std::uint64_t) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReading(devices.front().getKey(), "T", "TestValue"));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, AddReadingStringValues)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(),
                addReading(devices.front().getKey(), _, A<const std::vector<std::string>&>(), _))
      .WillOnce([&](const std::string&, const std::string&, const std::vector<std::string>&, std::uint64_t) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReading(devices.front().getKey(), "T", std::vector<std::string>{"TestValue"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, AddReadingSingleReading)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReading(devices.front().getKey(), A<const Reading&>()))
      .WillOnce([&](const std::string&, const Reading&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReading(devices.front().getKey(), Reading{"T", std::string{"TestValue"}}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, AddReadingMultipleReadings)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addReadings(devices.front().getKey(), A<const std::vector<Reading>&>()))
      .WillOnce([&](const std::string&, const std::vector<Reading>&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->addReadings(devices.front().getKey(), {Reading{"T", std::string{"TestValue"}}}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, AddFeedWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), registerFeed).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->registerFeed("TestDevice", Feed{"TestFeed", "TF", FeedType::IN_OUT, "NUMERIC"}));
}

TEST_F(WolkMultiTests, AddFeed)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), registerFeed(devices.front().getKey(), _))
      .WillOnce([&](const std::string&, const Feed&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(
      service->registerFeed(devices.front().getKey(), Feed{"TestFeed", "TF", FeedType::IN_OUT, "NUMERIC"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, AddFeedsWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), registerFeeds).Times(0);
    ASSERT_NO_FATAL_FAILURE(
      service->registerFeeds("TestDevice", {Feed{"TestFeed", "TF", FeedType::IN_OUT, "NUMERIC"}}));
}

TEST_F(WolkMultiTests, AddFeeds)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), registerFeeds(devices.front().getKey(), _))
      .WillOnce([&](const std::string&, const std::vector<Feed>&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(
      service->registerFeeds(devices.front().getKey(), {Feed{"TestFeed", "TF", FeedType::IN_OUT, "NUMERIC"}}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, RemoveFeedWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), removeFeed).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->removeFeed("TestDevice", "TestFeed"));
}

TEST_F(WolkMultiTests, RemoveFeed)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), removeFeed(devices.front().getKey(), _))
      .WillOnce([&](const std::string&, const std::string&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->removeFeed(devices.front().getKey(), "TestFeed"));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, RemoveFeedsWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), removeFeeds).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->removeFeeds("TestDevice", {"TestFeed"}));
}

TEST_F(WolkMultiTests, RemoveFeeds)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), removeFeeds(devices.front().getKey(), _))
      .WillOnce([&](const std::string&, const std::vector<std::string>&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->removeFeeds(devices.front().getKey(), {"TestFeed"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, PullFeedValuesWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), pullFeedValues).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->pullFeedValues("TestDevice"));
}

TEST_F(WolkMultiTests, PullFeedValues)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), pullFeedValues(devices.front().getKey())).WillOnce([&](const std::string&) {
        called = true;
        Notify();
    });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->pullFeedValues(devices.front().getKey()));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, PullParametersWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), pullParameters).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->pullParameters("TestDevice"));
}

TEST_F(WolkMultiTests, PullParameters)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), pullParameters(devices.front().getKey())).WillOnce([&](const std::string&) {
        called = true;
        Notify();
    });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(service->pullParameters(devices.front().getKey()));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, AddAttributeWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), addAttribute).Times(0);
    ASSERT_NO_FATAL_FAILURE(
      service->addAttribute("TestDevice", Attribute{"TestAttribute", DataType::STRING, "TestValue"}));
}

TEST_F(WolkMultiTests, AddAttribute)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), addAttribute(devices.front().getKey(), _))
      .WillOnce([&](const std::string&, const Attribute&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(
      service->addAttribute(devices.front().getKey(), Attribute{"TestAttribute", DataType::STRING, "TestValue"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, updateParameterWrongDevice)
{
    EXPECT_CALL(GetDataServiceReference(), updateParameter).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->updateParameter("TestDevice", Parameter{ParameterName::EXTERNAL_ID, "TestValue"}));
}

TEST_F(WolkMultiTests, UpdateParameter)
{
    // Set up the DataService to be called
    std::atomic_bool called{false};
    EXPECT_CALL(GetDataServiceReference(), updateParameter(devices.front().getKey(), _))
      .WillOnce([&](const std::string&, const Parameter&) {
          called = true;
          Notify();
      });

    // Call the service
    ASSERT_NO_FATAL_FAILURE(
      service->updateParameter(devices.front().getKey(), Parameter{ParameterName::EXTERNAL_ID, "TestValue"}));
    if (!called)
        Await();
    EXPECT_TRUE(called);
}

TEST_F(WolkMultiTests, RegisterDeviceWrongDevice)
{
    EXPECT_CALL(GetRegistrationServiceReference(), registerDevices).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->registerDevice("TestDevice", {}));
}

TEST_F(WolkMultiTests, RegisterDeviceReturnsError)
{
    EXPECT_CALL(GetRegistrationServiceReference(), registerDevices)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));
    ASSERT_FALSE(service->registerDevice(devices.front().getKey(), {}));
}

TEST_F(WolkMultiTests, RegisterDeviceHappyFlow)
{
    EXPECT_CALL(GetRegistrationServiceReference(), registerDevices).WillOnce(Return(ByMove(nullptr)));
    ASSERT_TRUE(service->registerDevice(devices.front().getKey(), {}));
}

TEST_F(WolkMultiTests, RegisterDeviceWrongDevices)
{
    EXPECT_CALL(GetRegistrationServiceReference(), registerDevices).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->registerDevices("TestDevice", {{}}));
}

TEST_F(WolkMultiTests, RegisterDevicesReturnsError)
{
    EXPECT_CALL(GetRegistrationServiceReference(), registerDevices)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));
    ASSERT_FALSE(service->registerDevices(devices.front().getKey(), {{}}));
}

TEST_F(WolkMultiTests, RegisterDevicesHappyFlow)
{
    EXPECT_CALL(GetRegistrationServiceReference(), registerDevices).WillOnce(Return(ByMove(nullptr)));
    ASSERT_TRUE(service->registerDevices(devices.front().getKey(), {{}}));
}

TEST_F(WolkMultiTests, RemoveDeviceWrongDevice)
{
    EXPECT_CALL(GetRegistrationServiceReference(), removeDevices).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->removeDevice("TestDevice", "DeviceKey"));
}

TEST_F(WolkMultiTests, RemoveDeviceReturnsError)
{
    EXPECT_CALL(GetRegistrationServiceReference(), removeDevices)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));
    ASSERT_FALSE(service->removeDevice(devices.front().getKey(), {}));
}

TEST_F(WolkMultiTests, RemoveDeviceHappyFlow)
{
    EXPECT_CALL(GetRegistrationServiceReference(), removeDevices).WillOnce(Return(ByMove(nullptr)));
    ASSERT_TRUE(service->removeDevice(devices.front().getKey(), {}));
}

TEST_F(WolkMultiTests, RemoveDevicesWrongDevice)
{
    EXPECT_CALL(GetRegistrationServiceReference(), removeDevices).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->removeDevices("TestDevice", {"DeviceKey"}));
}

TEST_F(WolkMultiTests, RemoveDevicesReturnsError)
{
    EXPECT_CALL(GetRegistrationServiceReference(), removeDevices)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));
    ASSERT_FALSE(service->removeDevices(devices.front().getKey(), {}));
}

TEST_F(WolkMultiTests, RemoveDevicesHappyFlow)
{
    EXPECT_CALL(GetRegistrationServiceReference(), removeDevices).WillOnce(Return(ByMove(nullptr)));
    ASSERT_TRUE(service->removeDevices(devices.front().getKey(), {}));
}

TEST_F(WolkMultiTests, ObtainDevicesWrongDevice)
{
    EXPECT_CALL(GetRegistrationServiceReference(), obtainDevices).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->obtainDevices("TestDevice", std::chrono::system_clock::now()));
}

TEST_F(WolkMultiTests, ObtainDevicesHappyFlow)
{
    EXPECT_CALL(GetRegistrationServiceReference(), obtainDevices).WillOnce(Return(ByMove(nullptr)));
    ASSERT_EQ(service->obtainDevices(devices.front().getKey(), std::chrono::system_clock::now()), nullptr);
}

TEST_F(WolkMultiTests, ObtainDevicesAsyncWrongDevice)
{
    EXPECT_CALL(GetRegistrationServiceReference(), obtainDevicesAsync).Times(0);
    ASSERT_FALSE(service->obtainDevicesAsync("TestDevice", std::chrono::system_clock::now()));
}

TEST_F(WolkMultiTests, ObtainDevicesAsyncHappyFlow)
{
    EXPECT_CALL(GetRegistrationServiceReference(), obtainDevicesAsync).WillOnce(Return(ByMove(true)));
    ASSERT_TRUE(service->obtainDevicesAsync(devices.front().getKey(), std::chrono::system_clock::now()));
}

TEST_F(WolkMultiTests, PeekErrorCountWrongDevice)
{
    EXPECT_CALL(GetErrorServiceReference(), peekMessagesForDevice).Times(0);
    ASSERT_EQ(service->peekErrorCount("TestDevice"), UINT64_MAX);
}

TEST_F(WolkMultiTests, PeekErrorCountHappyFlow)
{
    EXPECT_CALL(GetErrorServiceReference(), peekMessagesForDevice).WillOnce(Return(33));
    ASSERT_EQ(service->peekErrorCount(devices.front().getKey()), 33);
}

TEST_F(WolkMultiTests, DequeueMessageWrongDevice)
{
    EXPECT_CALL(GetErrorServiceReference(), obtainFirstMessageForDevice).Times(0);
    ASSERT_EQ(service->dequeueMessage("TestDevice"), nullptr);
}

TEST_F(WolkMultiTests, DequeueMessageHappyFlow)
{
    EXPECT_CALL(GetErrorServiceReference(), obtainFirstMessageForDevice)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));
    ASSERT_NE(service->dequeueMessage(devices.front().getKey()), nullptr);
}

TEST_F(WolkMultiTests, PopMessageWrongDevice)
{
    EXPECT_CALL(GetErrorServiceReference(), obtainLastMessageForDevice).Times(0);
    ASSERT_EQ(service->popMessage("TestDevice"), nullptr);
}

TEST_F(WolkMultiTests, PopMessageHappyFlow)
{
    EXPECT_CALL(GetErrorServiceReference(), obtainLastMessageForDevice).Times(1);
    ASSERT_EQ(service->popMessage(devices.front().getKey()), nullptr);
}

TEST_F(WolkMultiTests, ReportFilesForDeviceNoService)
{
    ASSERT_NO_FATAL_FAILURE(service->reportFilesForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFilesForDevice)
{
    SetUpFileManagement();
    EXPECT_CALL(GetFileManagementServiceReference(), reportPresentFiles(devices.front().getKey())).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->reportFilesForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFileManagementParametersForDeviceNoService)
{
    EXPECT_CALL(GetDataServiceReference(), updateParameter(devices.front().getKey(), _)).Times(2);
    ASSERT_NO_FATAL_FAILURE(service->reportFileManagementParametersForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFileManagementParametersForDevice)
{
    SetUpFileManagement();
    EXPECT_CALL(GetDataServiceReference(), updateParameter(devices.front().getKey(), _)).Times(2);
    ASSERT_NO_FATAL_FAILURE(service->reportFileManagementParametersForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFirmwareUpdateForDeviceNoService)
{
    ASSERT_NO_FATAL_FAILURE(service->reportFirmwareUpdateForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFirmwareUpdateForDeviceInstaller)
{
    SetUpFirmwareUpdateInstaller();
    GetFirmwareUpdateServiceReference().m_queue.push(std::make_shared<wolkabout::Message>("", ""));
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), loadState(devices.front().getKey())).Times(1);
    EXPECT_CALL(GetConnectivityServiceReference(), publish).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->reportFirmwareUpdateForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFirmwareUpdateForDeviceParameterListener)
{
    SetUpFirmwareUpdateParameterListener();
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), obtainParametersAndAnnounce(devices.front().getKey())).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->reportFirmwareUpdateForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFirmwareUpdateParametersForDeviceNoService)
{
    EXPECT_CALL(GetDataServiceReference(), updateParameter(devices.front().getKey(), _)).Times(2);
    ASSERT_NO_FATAL_FAILURE(service->reportFirmwareUpdateParametersForDevice(devices.front()));
}

TEST_F(WolkMultiTests, ReportFirmwareUpdateParametersForDevice)
{
    SetUpFirmwareUpdateInstaller();
    EXPECT_CALL(GetDataServiceReference(), updateParameter(devices.front().getKey(), _)).Times(2);
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), getVersionForDevice).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->reportFirmwareUpdateParametersForDevice(devices.front()));
}

TEST_F(WolkMultiTests, NotifyConnected)
{
    SetUpFileManagement();
    SetUpFirmwareUpdateInstaller();
    EXPECT_CALL(GetFileManagementServiceReference(), reportPresentFiles).Times(2);
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), loadState).Times(2);
    ASSERT_NO_FATAL_FAILURE(service->notifyConnected());
}

TEST_F(WolkMultiTests, AddDeviceAlreadyInList)
{
    ASSERT_FALSE(service->addDevice(devices.front()));
}

TEST_F(WolkMultiTests, AddDeviceConnected)
{
    SetUpFileManagement();
    SetUpFirmwareUpdateInstaller();
    service->m_connected = true;
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), getVersionForDevice).Times(1);
    EXPECT_CALL(GetFileManagementServiceReference(), reportPresentFiles).Times(1);
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), loadState).Times(1);
    EXPECT_CALL(GetDataServiceReference(), updateParameter).Times(4);
    EXPECT_CALL(GetDataServiceReference(), publishParameters(_)).Times(1);
    ASSERT_TRUE(service->addDevice(Device{"TestDevice3", "", OutboundDataMode::PUSH}));
}

TEST_F(WolkMultiTests, AddDeviceDisconnected)
{
    SetUpFileManagement();
    SetUpFirmwareUpdateInstaller();
    service->m_connected = false;
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), getVersionForDevice).Times(1);
    EXPECT_CALL(GetFileManagementServiceReference(), reportPresentFiles).Times(0);
    EXPECT_CALL(GetFirmwareUpdateServiceReference(), loadState).Times(0);
    EXPECT_CALL(GetDataServiceReference(), updateParameter).Times(4);
    EXPECT_CALL(GetDataServiceReference(), publishParameters(_)).Times(0);
    ASSERT_TRUE(service->addDevice(Device{"TestDevice3", "", OutboundDataMode::PUSH}));
}

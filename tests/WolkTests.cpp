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
#include "tests/mocks/FileManagementProtocolMock.h"
#include "tests/mocks/FileManagementServiceMock.h"
#include "tests/mocks/InboundMessageHandlerMock.h"
#include "tests/mocks/PersistenceMock.h"

#include <gtest/gtest.h>

using namespace wolkabout::connect;
using namespace ::testing;

class WolkTests : public Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    const std::string TAG = "WolkTests";

    std::shared_ptr<wolkabout::Device> device =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", OutboundDataMode::PUSH);

    std::shared_ptr<WolkBuilder> builder;

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
    builder = std::make_shared<WolkBuilder>(*device);
    const auto wolk = builder->build();

    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    EXPECT_NO_FATAL_FAILURE(wolk->notifyConnected());
    EXPECT_NO_FATAL_FAILURE(wolk->notifyDisconnected());
}

TEST_F(WolkTests, ConnectTest)
{
    builder = std::make_shared<WolkBuilder>(*device);
    const auto wolk = builder->build();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    auto feedHandler = [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) {};
    auto parameterHandler = [&](const std::string&, const std::vector<Parameter>&) {};

    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
      *dataProtocolMock, *persistenceMock, *connectivityServiceMock, feedHandler, parameterHandler));

    wolk->m_dataService = std::move(dataServiceMock);
    wolk->m_connectivityService = std::move(connectivityServiceMock);

    EXPECT_CALL(dynamic_cast<ConnectivityServiceMock&>(*(wolk->m_connectivityService)), connect)
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(this, &WolkTests::onEvent), testing::Return(true)));

    EXPECT_NO_FATAL_FAILURE(wolk->connect());

    waitEvents(1);
}

TEST_F(WolkTests, WhenConnected_PublishFileList)
{
    builder = std::make_shared<WolkBuilder>(*device);
    const auto wolk = builder->build();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    auto feedHandler = [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) {};
    auto parameterHandler = [&](const std::string&, const std::vector<Parameter>&) {};

    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
      *dataProtocolMock, *persistenceMock, *connectivityServiceMock, feedHandler, parameterHandler));

    auto fileManagementProtocolMock =
      std::unique_ptr<FileManagementProtocolMock>(new NiceMock<FileManagementProtocolMock>);
    auto fileManagementServiceMock = std::make_shared<FileManagementServiceMock>(
      *connectivityServiceMock, *dataServiceMock, *fileManagementProtocolMock, "./");

    wolk->m_dataService = std::move(dataServiceMock);
    wolk->m_connectivityService = std::move(connectivityServiceMock);
    wolk->m_fileManagementService = std::move(fileManagementServiceMock);

    ON_CALL(dynamic_cast<ConnectivityServiceMock&>(*(wolk->m_connectivityService)), connect)
      .WillByDefault(testing::Return(true));

    EXPECT_CALL(dynamic_cast<FileManagementServiceMock&>(*(wolk->m_fileManagementService)), reportPresentFiles)
      .WillOnce(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));

    EXPECT_NO_FATAL_FAILURE(wolk->connect());

    waitEvents(1);
}

TEST_F(WolkTests, DisconnectTest)
{
    builder = std::make_shared<WolkBuilder>(*device);
    const auto wolk = builder->build();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    wolk->m_connectivityService = std::move(connectivityServiceMock);

    EXPECT_NO_FATAL_FAILURE(wolk->disconnect());
}

TEST_F(WolkTests, AddingReadings)
{
    builder = std::make_shared<WolkBuilder>(*device);
    const auto wolk = builder->buildWolkSingle();

    auto dataProtocolMock = std::unique_ptr<DataProtocolMock>(new NiceMock<DataProtocolMock>());
    auto persistenceMock = std::unique_ptr<PersistenceMock>(new NiceMock<PersistenceMock>());
    auto connectivityServiceMock = std::unique_ptr<ConnectivityServiceMock>(new NiceMock<ConnectivityServiceMock>());

    auto feedHandler = [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) {};
    auto parameterHandler = [&](const std::string&, const std::vector<Parameter>&) {};

    auto dataServiceMock = std::unique_ptr<DataServiceMock>(new NiceMock<DataServiceMock>(
      *dataProtocolMock, *persistenceMock, *connectivityServiceMock, feedHandler, parameterHandler));

    wolk->m_dataService = std::move(dataServiceMock);

    EXPECT_CALL(
      dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)),
      addReading(A<const std::string&>(), A<const std::string&>(), A<const std::string&>(), A<std::uint64_t>()))
      .Times(1)
      .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));

    EXPECT_CALL(dynamic_cast<DataServiceMock&>(*(wolk->m_dataService)),
                addReading(A<const std::string&>(), A<const std::string&>(), A<const std::vector<std::string>&>(),
                           A<std::uint64_t>()))
      .Times(1)
      .WillRepeatedly(testing::InvokeWithoutArgs(this, &WolkTests::onEvent));

    EXPECT_NO_FATAL_FAILURE(wolk->addReading("TEST_REF1", 100));
    EXPECT_NO_FATAL_FAILURE(wolk->addReading("TEST_REF2", std::vector<int>{1, 2, 3}));
    // Empty test
    EXPECT_NO_FATAL_FAILURE(wolk->addReading("TEST_REF3", std::vector<int>{}));

    waitEvents(2);
}

TEST_F(WolkTests, HandlingReadings)
{
    builder = std::make_shared<WolkBuilder>(*device);

    // Make a lambda that will receive feed values
    auto handler = [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) { onEvent(); };

    builder->feedUpdateHandler(handler);

    const auto wolk = builder->build();

    EXPECT_NO_FATAL_FAILURE(wolk->handleFeedUpdateCommand("", {}));

    waitEvents(1);
}

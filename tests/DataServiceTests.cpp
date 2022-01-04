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
#include <utility>

#define private public
#define protected public
#include "wolk/service/data/DataService.h"
#undef private
#undef protected

#include "core/Types.h"
#include "core/model/Feed.h"
#include "core/utilities/Logger.h"
#include "tests/mocks/ConnectivityServiceMock.h"
#include "tests/mocks/DataProtocolMock.h"
#include "tests/mocks/PersistenceMock.h"

#include <gtest/gtest.h>

using namespace ::testing;
using namespace wolkabout;

class DataServiceTests : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Set up the mocks
        connectivityServiceMock = std::make_shared<ConnectivityServiceMock>();
        dataProtocolMock = std::make_shared<DataProtocolMock>();
        persistenceMock = std::make_shared<PersistenceMock>();

        // Set up the callback
        _internalFeedUpdateSetHandler =
          [&](const std::string& deviceKey, const std::map<std::uint64_t, std::vector<Reading>>& readings)
        {
            if (feedUpdateSetHandler)
                feedUpdateSetHandler(deviceKey, std::move(readings));
        };
        _internalParameterSyncHandler = [&](const std::string& deviceKey, const std::vector<Parameter>& parameters)
        {
            if (parameterSyncHandler)
                parameterSyncHandler(deviceKey, std::move(parameters));
        };

        // Create the service
        dataService = std::make_shared<DataService>(*dataProtocolMock, *persistenceMock, *connectivityServiceMock,
                                                    _internalFeedUpdateSetHandler, _internalParameterSyncHandler);
    }

    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    const std::string TAG = "DataServiceTests";

    const std::string DEVICE_KEY = "WOLK_CPP_TEST";

    FeedUpdateSetHandler feedUpdateSetHandler;

    ParameterSyncHandler parameterSyncHandler;

    std::shared_ptr<ConnectivityServiceMock> connectivityServiceMock;

    std::shared_ptr<DataProtocolMock> dataProtocolMock;

    std::shared_ptr<PersistenceMock> persistenceMock;

    std::shared_ptr<DataService> dataService;

    FeedUpdateSetHandler _internalFeedUpdateSetHandler;
    ParameterSyncHandler _internalParameterSyncHandler;
};

TEST_F(DataServiceTests, RegisterSingleFeedTest)
{
    // Define the single feed
    auto feed = Feed{"Test Feed", "T", FeedType::IN_OUT, Unit::AMPERE};

    // Set up expected calls for the protocol and the connectivity service
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRegistrationMessage>()))
      .WillOnce(
        [&](const std::string& /** deviceKey **/, const FeedRegistrationMessage& /** feedRegistrationMessage **/) {
            return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
        });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));

    // This time everything will succeed
    ASSERT_NO_FATAL_FAILURE(dataService->registerFeed(DEVICE_KEY, feed));
}

TEST_F(DataServiceTests, RegisterSingleFeedTestFailsToPublish)
{
    // Define the single feed
    auto feed = Feed{"Test Feed", "T", FeedType::IN_OUT, Unit::AMPERE};

    // Set up expected calls for the protocol and the connectivity service
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRegistrationMessage>()))
      .WillOnce(
        [&](const std::string& /** deviceKey **/, const FeedRegistrationMessage& /** feedRegistrationMessage **/) {
            return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
        });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));

    // This time everything will succeed
    ASSERT_NO_FATAL_FAILURE(dataService->registerFeed(DEVICE_KEY, feed));
}

TEST_F(DataServiceTests, RegisterSingleFeedTestFailsToParse)
{
    // Define the single feed
    auto feed = Feed{"Test Feed", "T", FeedType::IN_OUT, Unit::AMPERE};

    // Set up expected calls for the protocol and the connectivity service
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRegistrationMessage>()))
      .WillOnce([&](const std::string& /** deviceKey **/,
                    const FeedRegistrationMessage& /** feedRegistrationMessage **/) { return nullptr; });
    EXPECT_CALL(*connectivityServiceMock, publish).Times(0);

    // This time everything will succeed
    ASSERT_NO_FATAL_FAILURE(dataService->registerFeed(DEVICE_KEY, feed));
}

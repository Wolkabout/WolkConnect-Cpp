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
#include "wolk/service/platform_status/PlatformStatusService.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/PlatformStatusProtocolMock.h"
#include "tests/mocks/PlatformStatusListenerMock.h"

#include <gtest/gtest.h>

using namespace ::testing;
using namespace wolkabout;

class PlatformStatusServiceTests : public ::testing::Test
{
public:
    void SetUp() override
    {
        platformStatusListenerMock = new PlatformStatusListenerMock;
        auto listenerMock = std::unique_ptr<PlatformStatusListenerMock>{platformStatusListenerMock};
        service = std::unique_ptr<PlatformStatusService>{
          new PlatformStatusService{platformStatusProtocolMock, std::move(listenerMock)}};
    }

    // No need to delete the listener mock, as it's now being deleted by the `std::unique_ptr::~unique_ptr()`.

    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    std::unique_ptr<PlatformStatusService> service;

    PlatformStatusProtocolMock platformStatusProtocolMock;
    PlatformStatusListenerMock* platformStatusListenerMock;
};

TEST_F(PlatformStatusServiceTests, ProtocolCheck)
{
    EXPECT_EQ(&(service->getProtocol()), &platformStatusProtocolMock);
}

TEST_F(PlatformStatusServiceTests, FailedToParseIncomingMessage)
{
    // Set up the protocol to fail to parse the message
    EXPECT_CALL(platformStatusProtocolMock, parsePlatformStatusMessage).WillOnce(Return(ByMove(nullptr)));

    // Pass the message
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(PlatformStatusServiceTests, ParsedIncomingMessageCalledListener)
{
    // Set up the protocol to parse the message properly
    auto status = ConnectivityStatus::CONNECTED;
    EXPECT_CALL(platformStatusProtocolMock, parsePlatformStatusMessage)
      .WillOnce(Return(ByMove(std::unique_ptr<PlatformStatusMessage>{new PlatformStatusMessage{status}})));

    // Except the listener to be called
    std::atomic_bool called{false};
    std::mutex mutex;
    std::condition_variable conditionVariable;
    EXPECT_CALL(*platformStatusListenerMock, platformStatus(ConnectivityStatus::CONNECTED))
      .WillOnce(
        [&](ConnectivityStatus)
        {
            called = true;
            conditionVariable.notify_one();
        });

    // Pass the message
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
    if (!called)
    {
        auto lock = std::unique_lock<std::mutex>{mutex};
        conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
    }
}

TEST_F(PlatformStatusServiceTests, ParsedIncomingMessageCalledCallback)
{
    // Create the callback that will be invoked
    std::atomic_bool called{false};
    std::mutex mutex;
    std::condition_variable conditionVariable;
    auto status = ConnectivityStatus::CONNECTED;
    auto callback = [&](ConnectivityStatus received)
    {
        if (received == wolkabout::ConnectivityStatus::CONNECTED)
        {
            called = true;
            conditionVariable.notify_one();
        }
    };

    // Make the service with the callback
    service.reset(new PlatformStatusService{platformStatusProtocolMock, callback});

    // Set up the protocol
    EXPECT_CALL(platformStatusProtocolMock, parsePlatformStatusMessage)
      .WillOnce(Return(ByMove(std::unique_ptr<PlatformStatusMessage>{new PlatformStatusMessage{status}})));

    // Pass the message
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));

    // Now expect the callback to get called
    if (!called)
    {
        const auto timeout = std::chrono::milliseconds{100};
        auto lock = std::unique_lock<std::mutex>{mutex};
        conditionVariable.wait_for(lock, timeout);
    }
    EXPECT_TRUE(called);
}

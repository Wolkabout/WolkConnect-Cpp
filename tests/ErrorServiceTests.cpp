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
#include "wolk/service/error/ErrorService.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/ErrorProtocolMock.h"

#include <gtest/gtest.h>

using namespace ::testing;
using namespace wolkabout;

class ErrorServiceTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    void SetUp() override { service = std::unique_ptr<ErrorService>{new ErrorService{errorProtocolMock, RETAIN_TIME}}; }
    void TearDown() override { service.reset(); }

    void addTestMessageToService()
    {
        // Add a single message into the cache
        auto errorMessage =
          std::unique_ptr<ErrorMessage>{new ErrorMessage{DEVICE_KEY, TEST_CONTENT, std::chrono::system_clock::now()}};

        // Pass it into the map
        service->m_cached.emplace(DEVICE_KEY, DeviceErrorMessages{});
        service->m_cached[DEVICE_KEY].emplace(std::chrono::system_clock::now(), std::move(errorMessage));
        ASSERT_FALSE(service->m_cached.empty());
    }

    void sendMessageToService()
    {
        // Send the message to the service
        auto errorMessage = std::make_shared<wolkabout::Message>("Test Error Message", "p2d/" + DEVICE_KEY + "/error");

        // Pass it to the service
        service->messageReceived(errorMessage);
    }

    ErrorProtocolMock errorProtocolMock;

    std::unique_ptr<ErrorService> service;

    const std::string DEVICE_KEY = "TestDevice";

    const std::string TEST_CONTENT = "Test Error Message!";

    const std::chrono::milliseconds RETAIN_TIME = std::chrono::milliseconds{33};
};

TEST_F(ErrorServiceTests, GetTheProtocolOmg)
{
    EXPECT_EQ(&service->getProtocol(), &errorProtocolMock);
}

TEST_F(ErrorServiceTests, InvalidMessageReceived)
{
    ASSERT_TRUE(service->m_cached.empty());
    EXPECT_CALL(errorProtocolMock, parseError).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("Ping", "Pong")));
    ASSERT_TRUE(service->m_cached.empty());
}

TEST_F(ErrorServiceTests, OneMessageInCacheDeleted)
{
    // Add the message and start the timer
    addTestMessageToService();
    service->start();

    // Wait for retain time (plus 25% of the time, just to make sure)
    std::this_thread::sleep_for(RETAIN_TIME * 1.25);
    EXPECT_TRUE(service->m_cached[DEVICE_KEY].empty());
}

TEST_F(ErrorServiceTests, OneMessageObtainFromCache)
{
    // Add the message
    addTestMessageToService();

    // Obtain the message from the cache
    auto message = std::unique_ptr<ErrorMessage>{};
    ASSERT_NO_FATAL_FAILURE(message =
                              service->obtainOrAwaitMessageForDevice(DEVICE_KEY, std::chrono::milliseconds{100}));
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->getDeviceKey(), DEVICE_KEY);
    EXPECT_EQ(message->getMessage(), TEST_CONTENT);
    EXPECT_NE(message->getArrivalTime().time_since_epoch().count(), 0);
}

TEST_F(ErrorServiceTests, OneMessageObtainFromCacheMapExistsButNoMessage)
{
    // Add the message and start the timer
    addTestMessageToService();
    service->start();

    // Wait for retain time (plus 25% of the time, just to make sure)
    std::this_thread::sleep_for(RETAIN_TIME * 1.25);
    ASSERT_EQ(service->obtainFirstMessageForDevice(DEVICE_KEY), nullptr);
}

TEST_F(ErrorServiceTests, ObtainLastNeverThere)
{
    ASSERT_EQ(service->obtainLastMessageForDevice(DEVICE_KEY), nullptr);
}

TEST_F(ErrorServiceTests, OneMessageObtainLastFromCache)
{
    // Add the message
    addTestMessageToService();

    // Obtain the message from the cache
    auto message = std::unique_ptr<ErrorMessage>{};
    ASSERT_NO_FATAL_FAILURE(message = service->obtainLastMessageForDevice(DEVICE_KEY));
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->getDeviceKey(), DEVICE_KEY);
    EXPECT_EQ(message->getMessage(), TEST_CONTENT);
    EXPECT_NE(message->getArrivalTime().time_since_epoch().count(), 0);
}

TEST_F(ErrorServiceTests, OneMessageObtainLastFromCacheMapExistsButNoMessage)
{
    // Add the message and start the timer
    addTestMessageToService();
    service->start();

    // Wait for retain time (plus 25% of the time, just to make sure)
    std::this_thread::sleep_for(RETAIN_TIME * 1.25);
    ASSERT_EQ(service->obtainLastMessageForDevice(DEVICE_KEY), nullptr);
}

TEST_F(ErrorServiceTests, AwaitOneMessageTest)
{
    // Prepare a task that will add the message
    auto delay = std::chrono::milliseconds{25};
    Timer timer;
    timer.start(delay, [&]() {
        addTestMessageToService();
        service->m_conditionVariables[DEVICE_KEY]->notify_one();
    });

    // Now await the message
    auto start = std::chrono::system_clock::now();
    auto message = std::unique_ptr<ErrorMessage>{};
    ASSERT_NO_FATAL_FAILURE(message = service->obtainOrAwaitMessageForDevice(DEVICE_KEY, std::chrono::seconds(1)));
    auto duration = std::chrono::system_clock::now() - start;
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->getDeviceKey(), DEVICE_KEY);
    EXPECT_EQ(message->getMessage(), TEST_CONTENT);
    EXPECT_NE(message->getArrivalTime().time_since_epoch().count(), 0);
}

TEST_F(ErrorServiceTests, FullMessageArrivalTest)
{
    // Prepare a task that will add the message
    auto delay = std::chrono::milliseconds{33};
    Timer timer;
    timer.start(delay, [&]() { sendMessageToService(); });

    // Except the call on the mock
    EXPECT_CALL(errorProtocolMock, parseError)
      .WillOnce(Return(ByMove(
        std::unique_ptr<ErrorMessage>{new ErrorMessage{DEVICE_KEY, TEST_CONTENT, std::chrono::system_clock::now()}})));

    // Now await the message
    auto start = std::chrono::system_clock::now();
    auto message = std::unique_ptr<ErrorMessage>{};
    ASSERT_NO_FATAL_FAILURE(message = service->obtainOrAwaitMessageForDevice(DEVICE_KEY, std::chrono::seconds(1)));
    auto duration = std::chrono::system_clock::now() - start;
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(message->getDeviceKey(), DEVICE_KEY);
    EXPECT_EQ(message->getMessage(), TEST_CONTENT);
    EXPECT_NE(message->getArrivalTime().time_since_epoch().count(), 0);
}

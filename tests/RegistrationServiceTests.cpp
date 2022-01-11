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
#include "wolk/service/registration_service/RegistrationService.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/ConnectivityServiceMock.h"
#include "tests/mocks/ErrorProtocolMock.h"
#include "tests/mocks/ErrorServiceMock.h"
#include "tests/mocks/RegistrationProtocolMock.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace wolkabout;
using namespace ::testing;

class RegistrationServiceTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    void SetUp() override
    {
        connectivityServiceMock = std::make_shared<NiceMock<ConnectivityServiceMock>>();
        errorServiceMock = std::make_shared<NiceMock<ErrorServiceMock>>(errorProtocolMock, RETAIN_TIME);
        service = std::unique_ptr<RegistrationService>{
          new RegistrationService{registrationProtocolMock, *connectivityServiceMock, *errorServiceMock}};
        service->start();
    }

    void TearDown() override { service.reset(); }

    const std::string DEVICE_KEY = "TestDevice";

    const std::chrono::milliseconds RETAIN_TIME = std::chrono::milliseconds{500};

    ErrorProtocolMock errorProtocolMock;

    RegistrationProtocolMock registrationProtocolMock;

    std::shared_ptr<ConnectivityServiceMock> connectivityServiceMock;

    std::shared_ptr<ErrorServiceMock> errorServiceMock;

    std::unique_ptr<RegistrationService> service;

    Timer timer;
};

TEST_F(RegistrationServiceTests, Protocol)
{
    EXPECT_EQ(&(service->getProtocol()), &registrationProtocolMock);
}

TEST_F(RegistrationServiceTests, RegisterDevicesNotRunning)
{
    // Stop the service
    service->stop();

    // Call the service
    ASSERT_NE(service->registerDevices(DEVICE_KEY, {}), nullptr);
}

TEST_F(RegistrationServiceTests, RegisterDevicesEmptyVector)
{
    // Call the service
    ASSERT_NE(service->registerDevices(DEVICE_KEY, {}), nullptr);
}

TEST_F(RegistrationServiceTests, RegisterDevicesFailToParse)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRegistrationMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));

    // Call the service
    ASSERT_NE(service->registerDevices(DEVICE_KEY, {DeviceRegistrationData{"DeviceName", "DeviceKey", "", {}, {}, {}}}),
              nullptr);
}

TEST_F(RegistrationServiceTests, RegisterDevicesCouldNotSend)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRegistrationMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));

    // Call the service
    ASSERT_NE(service->registerDevices(DEVICE_KEY, {DeviceRegistrationData{"DeviceName", "DeviceKey", "", {}, {}, {}}}),
              nullptr);
}

TEST_F(RegistrationServiceTests, RegisterDevicesSentSuccessfully)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRegistrationMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));

    // Call the service
    ASSERT_EQ(service->registerDevices(DEVICE_KEY, {DeviceRegistrationData{"DeviceName", "DeviceKey", "", {}, {}, {}}}),
              nullptr);
}

TEST_F(RegistrationServiceTests, RegisterDevicesReceiveError)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRegistrationMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));

    // Set up the error service to return an error message
    const auto delay = std::chrono::milliseconds{50};
    EXPECT_CALL(*errorServiceMock, peekMessagesForDevice).WillOnce(Return(0));
    EXPECT_CALL(*errorServiceMock, awaitMessage).WillOnce([&](const std::string&, std::chrono::milliseconds) {
        std::this_thread::sleep_for(delay);
        return true;
    });
    EXPECT_CALL(*errorServiceMock, obtainFirstMessageForDevice)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));

    // Call the service and measure the execution time
    const auto start = std::chrono::system_clock::now();
    ASSERT_NE(service->registerDevices(DEVICE_KEY, {DeviceRegistrationData{"DeviceName", "DeviceKey", "", {}, {}, {}}}),
              nullptr);
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    const auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
}

TEST_F(RegistrationServiceTests, RemoveDevicesNotRunning)
{
    // Stop the service
    service->stop();

    // Call the service
    ASSERT_NE(service->removeDevices(DEVICE_KEY, {}), nullptr);
}

TEST_F(RegistrationServiceTests, RemoveDevicesEmptyVector)
{
    // Call the service
    ASSERT_NE(service->removeDevices(DEVICE_KEY, {}), nullptr);
}

TEST_F(RegistrationServiceTests, RemoveDevicesFailToParse)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRemovalMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));

    // Call the service
    ASSERT_NE(service->removeDevices(DEVICE_KEY, {"DeviceKey"}), nullptr);
}

TEST_F(RegistrationServiceTests, RemoveDevicesCouldNotSend)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRemovalMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));

    // Call the service
    ASSERT_NE(service->removeDevices(DEVICE_KEY, {"DeviceKey"}), nullptr);
}

TEST_F(RegistrationServiceTests, RemoveDevicesSentSuccessfully)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRemovalMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));

    // Call the service
    ASSERT_EQ(service->removeDevices(DEVICE_KEY, {"DeviceKey"}), nullptr);
}

TEST_F(RegistrationServiceTests, RemoveDevicesReceiveError)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const DeviceRemovalMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));

    // Set up the error service to return an error message
    const auto delay = std::chrono::milliseconds{50};
    EXPECT_CALL(*errorServiceMock, peekMessagesForDevice).WillOnce(Return(0));
    EXPECT_CALL(*errorServiceMock, awaitMessage).WillOnce([&](const std::string&, std::chrono::milliseconds) {
        std::this_thread::sleep_for(delay);
        return true;
    });
    EXPECT_CALL(*errorServiceMock, obtainFirstMessageForDevice)
      .WillOnce(
        Return(ByMove(std::unique_ptr<ErrorMessage>{new ErrorMessage{"", "", std::chrono::system_clock::now()}})));

    // Call the service and measure the execution time
    const auto start = std::chrono::system_clock::now();
    ASSERT_NE(service->removeDevices(DEVICE_KEY, {"DeviceKey"}), nullptr);
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    const auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
}

TEST_F(RegistrationServiceTests, ObtainDevicesNotRunning)
{
    // Stop the service
    service->stop();

    // Call the service (sync)
    ASSERT_EQ(service->obtainDevices(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60)), nullptr);
    EXPECT_TRUE(service->m_responses.empty());

    // Call the service (async)
    ASSERT_EQ(service->obtainDevicesAsync(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60)),
              false);
    EXPECT_TRUE(service->m_responses.empty());
}

TEST_F(RegistrationServiceTests, ObtainDevicesAsyncNoCallback)
{
    // Call the service (async)
    ASSERT_EQ(service->obtainDevicesAsync(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60)),
              false);
    EXPECT_TRUE(service->m_responses.empty());
}

TEST_F(RegistrationServiceTests, ObtainDevicesFailedToFormMessage)
{
    // Make the protocol return a nullptr for the parsing
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const RegisteredDevicesRequestMessage&>()))
      .WillOnce(Return(ByMove(nullptr)))
      .WillOnce(Return(ByMove(nullptr)));

    // Call the service (sync)
    ASSERT_EQ(service->obtainDevices(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60)), nullptr);
    EXPECT_TRUE(service->m_responses.empty());

    // Call the service (async)
    ASSERT_EQ(service->obtainDevicesAsync(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60), {},
                                          {}, [&](const std::vector<RegisteredDeviceInformation>&) {}),
              false);
    EXPECT_TRUE(service->m_responses.empty());
}

TEST_F(RegistrationServiceTests, ObtainDevicesFailedToPublish)
{
    // Make the protocol return a valid message, but the connectivity service not being able to send it.
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const RegisteredDevicesRequestMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillRepeatedly(Return(false));

    // Call the service (sync)
    ASSERT_EQ(service->obtainDevices(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60)), nullptr);
    EXPECT_TRUE(service->m_responses.empty());

    // Call the service (async)
    ASSERT_EQ(service->obtainDevicesAsync(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60), {},
                                          {}, [&](const std::vector<RegisteredDeviceInformation>&) {}),
              false);
    EXPECT_TRUE(service->m_responses.empty());
}

TEST_F(RegistrationServiceTests, ObtainDevicesNothingGotPushed)
{
    // Make the message valid
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const RegisteredDevicesRequestMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    const auto timeout = std::chrono::milliseconds{100};

    // Call the service
    const auto start = std::chrono::system_clock::now();
    ASSERT_EQ(
      service->obtainDevices(DEVICE_KEY, std::chrono::system_clock::now() - std::chrono::seconds(60), {}, {}, timeout),
      nullptr);
    EXPECT_TRUE(service->m_responses.empty());
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << timeout.count() << "ms.";
    EXPECT_GE(durationMs, timeout);
}

TEST_F(RegistrationServiceTests, ObtainDevicesExecutionAborted)
{
    // Make the message valid
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const RegisteredDevicesRequestMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    const auto timeout = std::chrono::milliseconds{100};

    // Make a delayed stop to the service
    const auto delay = std::chrono::milliseconds{50};
    timer.start(delay, [&] { service->stop(); });

    // And measure the execution time
    const auto start = std::chrono::system_clock::now();
    EXPECT_EQ(service->obtainDevices(DEVICE_KEY, std::chrono::system_clock::now(), {}, {}, timeout), nullptr);
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    const auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
}

TEST_F(RegistrationServiceTests, ObtainDevicesPushedArrayNoCallback)
{
    // Make the message valid
    const auto timeFrom =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const RegisteredDevicesRequestMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(registrationProtocolMock, parseRegisteredDevicesResponse)
      .WillOnce(Return(ByMove(std::unique_ptr<RegisteredDevicesResponseMessage>{new RegisteredDevicesResponseMessage{
        timeFrom, "", "", std::vector<RegisteredDeviceInformation>{{"DeviceKey", "", ""}}}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    const auto timeout = std::chrono::milliseconds{100};

    // Make a response arrive 50ms after the send
    const auto delay = std::chrono::milliseconds{50};
    timer.start(delay, [&] {
        service->messageReceived(
          std::make_shared<wolkabout::Message>("p2d/" + DEVICE_KEY + "/registered_devices",
                                               std::string(R"({"timestampFrom":)" + std::to_string(timeFrom.count())) +
                                                 R"(,"deviceType":"","externalId","","matchingDevices":[]})"));
    });

    // Call the service to expect the response
    const auto start = std::chrono::system_clock::now();
    auto vector = std::unique_ptr<std::vector<RegisteredDeviceInformation>>{};
    ASSERT_NO_FATAL_FAILURE(vector = service->obtainDevices(DEVICE_KEY, TimePoint(timeFrom), {}, {}, timeout));
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    const auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
    ASSERT_NE(vector, nullptr);
    ASSERT_FALSE(vector->empty());
}

TEST_F(RegistrationServiceTests, ObtainDevicesPushedDevicesInArrayWithCallback)
{
    // Make the message valid
    const auto timeFrom =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    EXPECT_CALL(registrationProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const RegisteredDevicesRequestMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(registrationProtocolMock, parseRegisteredDevicesResponse)
      .WillOnce(Return(ByMove(std::unique_ptr<RegisteredDevicesResponseMessage>{new RegisteredDevicesResponseMessage{
        timeFrom, "", "", std::vector<RegisteredDeviceInformation>{{"DeviceKey", "", ""}}}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    const auto timeout = std::chrono::milliseconds{100};

    // Make a response arrive 50ms after the send
    const auto delay = std::chrono::milliseconds{50};
    timer.start(delay, [&] {
        service->messageReceived(
          std::make_shared<wolkabout::Message>("p2d/" + DEVICE_KEY + "/registered_devices",
                                               std::string(R"({"timestampFrom":)" + std::to_string(timeFrom.count())) +
                                                 R"(,"deviceType":"","externalId","","matchingDevices":]})"));
    });

    // Make the mutex and the condition variable
    auto called = false;
    std::mutex mutex;
    std::condition_variable conditionVariable;

    // Call the service to expect the response
    const auto start = std::chrono::system_clock::now();
    ASSERT_NO_FATAL_FAILURE(service->obtainDevicesAsync(DEVICE_KEY, TimePoint(timeFrom), {}, {},
                                                        [&](const std::vector<RegisteredDeviceInformation>& vector) {
                                                            called = true;
                                                            EXPECT_FALSE(vector.empty());
                                                            conditionVariable.notify_one();
                                                        }));

    // Wait for the condition variable to be done waiting
    auto lock = std::unique_lock<std::mutex>{mutex};
    conditionVariable.wait_for(lock, timeout);
    ASSERT_TRUE(called);

    // Check the execution time
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    const auto tolerable = delay * 1.25;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs, tolerable);
}

TEST_F(RegistrationServiceTests, ReceivedMessageFailsToParse)
{
    // Make the protocol fail to parse the message
    EXPECT_CALL(registrationProtocolMock, parseRegisteredDevicesResponse).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(RegistrationServiceTests, ReceivedMessageNoRecord)
{
    // Make the protocol fail to parse the message
    EXPECT_CALL(registrationProtocolMock, parseRegisteredDevicesResponse)
      .WillOnce(Return(ByMove(std::unique_ptr<RegisteredDevicesResponseMessage>{new RegisteredDevicesResponseMessage{
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()), "",
        "", std::vector<RegisteredDeviceInformation>{}}})));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

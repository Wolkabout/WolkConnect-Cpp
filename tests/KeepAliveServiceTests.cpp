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
#include "service/KeepAliveService.h"
#include "model/DeviceStatus.h"
#include "model/Message.h"
#undef private
#undef protected

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>

#include "mocks/ConnectivityServiceMock.h"
#include "mocks/StatusProtocolMock.h"

class KeepAliveServiceTests : public ::testing::Test
{
public:
    void SetUp()
    {
        connectivityServiceMock.reset(new ::testing::NiceMock<ConnectivityServiceMock>());
        statusProtocolMock.reset(new ::testing::NiceMock<StatusProtocolMock>());
    }

    void TearDown()
    {
        connectivityServiceMock.reset();
        statusProtocolMock.reset();
    }

    std::unique_ptr<ConnectivityServiceMock> connectivityServiceMock;
    std::unique_ptr<StatusProtocolMock> statusProtocolMock;
};

using namespace ::testing;

TEST_F(KeepAliveServiceTests, SimpleCtorTest)
{
    std::unique_ptr<wolkabout::Message> message =
      std::unique_ptr<wolkabout::Message>(new wolkabout::Message("TEST", "TEST"));

    EXPECT_CALL(*statusProtocolMock, makeLastWillMessage({"TEST_KEY"})).WillOnce(Return(ByMove(std::move(message))));
    EXPECT_CALL(*connectivityServiceMock, setUncontrolledDisonnectMessage).WillOnce(Return());

    EXPECT_NO_THROW(const auto& keepAliveService = wolkabout::KeepAliveService(
                      "TEST_KEY", *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(1)));
}

TEST_F(KeepAliveServiceTests, OtherMethodsTest)
{
    std::shared_ptr<wolkabout::KeepAliveService> keepAliveService;
    ASSERT_NO_THROW(keepAliveService = std::make_shared<wolkabout::KeepAliveService>(
                      "TEST_KEY", *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(1)));

    const auto& message = std::make_shared<wolkabout::Message>("TEST", "TEST");
    EXPECT_NO_THROW(keepAliveService->messageReceived(message));

    //    std::cout << "KeepAliveServiceTests: " << &keepAliveService->getProtocol() << ", " << &(*statusProtocolMock)
    //    << std::endl;
    EXPECT_EQ(&keepAliveService->getProtocol(), &(*statusProtocolMock));
}

TEST_F(KeepAliveServiceTests, FunctionalityTest)
{
    std::shared_ptr<wolkabout::KeepAliveService> keepAliveService;
    ASSERT_NO_THROW(keepAliveService = std::make_shared<wolkabout::KeepAliveService>(
                      "TEST_KEY", *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(1)));

    std::unique_ptr<wolkabout::Message> message =
      std::unique_ptr<wolkabout::Message>(new wolkabout::Message("TEST", "TEST"));
    EXPECT_CALL(*statusProtocolMock, makeFromPingRequest)
      .WillOnce(Return(ByMove(std::move(message))));
    EXPECT_CALL(*connectivityServiceMock, publish).WillRepeatedly(Return(true));

    EXPECT_NO_THROW(keepAliveService->connected());

    std::this_thread::sleep_for(std::chrono::milliseconds (999));

    EXPECT_NO_THROW(keepAliveService->disconnected());
}

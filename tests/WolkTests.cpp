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
#include "mocks/KeepAliveServiceMock.h"
#include "mocks/StatusProtocolMock.h"

#include "model/DeviceStatus.h"
#include "model/Device.h"
#include "model/Message.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class WolkTests : public ::testing::Test
{
public:
    std::shared_ptr<wolkabout::Device> noKeyDevice =
      std::make_shared<wolkabout::Device>("", "", std::vector<std::string>());
    std::shared_ptr<wolkabout::Device> noActuatorsDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>());
    std::shared_ptr<wolkabout::Device> device =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    std::shared_ptr<wolkabout::WolkBuilder> builder;
};

TEST_F(WolkTests, NotifiesConnectDisconnect)
{
    builder = std::make_shared<wolkabout::WolkBuilder>(*noActuatorsDevice);
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

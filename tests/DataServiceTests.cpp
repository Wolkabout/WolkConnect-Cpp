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
#include "service/DataService.h"
#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ConfigurationSetCommand.h"
#include "model/Message.h"
#undef private
#undef protected

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

#include "mocks/ConnectivityServiceMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/PersistenceMock.h"

class DataServiceTests : public ::testing::Test
{
public:
    void SetUp()
    {
        dataProtocolMock.reset(new ::testing::NiceMock<DataProtocolMock>());
        persistenceMock.reset(new ::testing::NiceMock<PersistenceMock>());
        connectivityServiceMock.reset(new ::testing::NiceMock<ConnectivityServiceMock>());
    }

    void TearDown()
    {
        dataProtocolMock.reset();
        persistenceMock.reset();
        connectivityServiceMock.reset();
    }

    std::unique_ptr<DataProtocolMock> dataProtocolMock;
    std::unique_ptr<PersistenceMock> persistenceMock;
    std::unique_ptr<ConnectivityServiceMock> connectivityServiceMock;
};

using namespace ::testing;

TEST_F(DataServiceTests, SimpleCtorTest)
{
    EXPECT_NO_THROW(wolkabout::DataService("TEST_DEVICE_KEY", *dataProtocolMock, *persistenceMock,
                                           *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr));
}

TEST_F(DataServiceTests, MessageChecksTests)
{
    const auto& key = "TEST_DEVICE_KEY";
    std::unique_ptr<wolkabout::DataService> dataService;
    EXPECT_NO_THROW(dataService = std::unique_ptr<wolkabout::DataService>(
                      new wolkabout::DataService(key, *dataProtocolMock, *persistenceMock,
                                                 *connectivityServiceMock, nullptr, nullptr, nullptr, nullptr)));

    auto message = std::make_shared<wolkabout::Message>("DATA", key);

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(key)).WillOnce(Return(""));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(key)).WillOnce(Return("SOMETHING_DIFFERENT"));
    EXPECT_NO_THROW(dataService->messageReceived(message));

    EXPECT_CALL(*dataProtocolMock, extractDeviceKeyFromChannel(key)).WillOnce(Return(key));
    EXPECT_NO_THROW(dataService->messageReceived(message));
}

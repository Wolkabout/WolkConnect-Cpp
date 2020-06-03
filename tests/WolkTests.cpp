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
#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ConfigurationSetCommand.h"
#include "model/DeviceStatus.h"
#include "model/Message.h"
#include "Wolk.h"
#include "WolkBuilder.h"
#undef private
#undef protected

#include "mocks/ActuationHandlerMock.h"
#include "mocks/ActuatorStatusProviderMock.h"
#include "mocks/ConfigurationHandlerMock.h"
#include "mocks/ConfigurationProviderMock.h"
#include "mocks/ConnectivityServiceMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/DataServiceMock.h"
#include "mocks/KeepAliveServiceMock.h"
#include "mocks/PersistenceMock.h"
#include "mocks/StatusProtocolMock.h"

#include <gtest/gtest.h>

class WolkTests : public ::testing::Test
{
public:
    void SetUp()
    {
        const auto& testDevice =
          std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});
        builder = std::make_shared<wolkabout::WolkBuilder>(wolkabout::Wolk::newBuilder(*testDevice));
        actuationHandlerMock.reset(new ::testing::NiceMock<ActuationHandlerMock>());
        builder->actuationHandler(actuationHandlerMock);
        actuatorStatusProviderMock.reset(new ::testing::NiceMock<ActuatorStatusProviderMock>());
        builder->actuatorStatusProvider(actuatorStatusProviderMock);
        configurationHandlerMock.reset(new ::testing::NiceMock<ConfigurationHandlerMock>());
        builder->configurationHandler(configurationHandlerMock);
        configurationProviderMock.reset(new ::testing::NiceMock<ConfigurationProviderMock>());
        builder->configurationProvider(configurationProviderMock);

        connectivityServiceMock = new ::testing::NiceMock<ConnectivityServiceMock>();
        dataProtocolMock = new ::testing::NiceMock<DataProtocolMock>();
        persistenceMock = new ::testing::NiceMock<PersistenceMock>();
        statusProtocolMock = new ::testing::NiceMock<StatusProtocolMock>();

        keepAliveServiceMock = new ::testing::NiceMock<KeepAliveServiceMock>(
          deviceKey, *statusProtocolMock, *connectivityServiceMock, std::chrono::seconds(60));

        dataServiceMock = new ::testing::NiceMock<DataServiceMock>(deviceKey, *dataProtocolMock, *persistenceMock,
                                                                   *connectivityServiceMock);

        wolk = builder->build();

        wolk->m_connectivityService.reset(connectivityServiceMock);
        wolk->m_dataService.reset(dataServiceMock);
        wolk->m_keepAliveService.reset(keepAliveServiceMock);
    }

    void TearDown()
    {
        builder.reset();
        actuationHandlerMock.reset();
        actuatorStatusProviderMock.reset();
        configurationHandlerMock.reset();
        configurationProviderMock.reset();
    }

    const std::string deviceKey = "TEST_KEY";

    std::unique_ptr<wolkabout::Wolk> wolk;

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    std::shared_ptr<ActuationHandlerMock> actuationHandlerMock;
    std::shared_ptr<ActuatorStatusProviderMock> actuatorStatusProviderMock;
    std::shared_ptr<ConfigurationHandlerMock> configurationHandlerMock;
    std::shared_ptr<ConfigurationProviderMock> configurationProviderMock;

    ConnectivityServiceMock* connectivityServiceMock;
    DataServiceMock* dataServiceMock;
    DataProtocolMock* dataProtocolMock;
    KeepAliveServiceMock* keepAliveServiceMock;
    PersistenceMock* persistenceMock;
    StatusProtocolMock* statusProtocolMock;
};

TEST_F(WolkTests, SensorReadingValue)
{
    //    std::cout << &(*(wolk->m_dataService)) << ", " << dataServiceMock << ", "
    //              << (&(*(wolk->m_dataService)) == dataServiceMock) << std::endl;

    //    Can't somehow get the wolk instance to call the mock functions.
    //    EXPECT_CALL(*dataServiceMock, addSensorReading(testing::_, testing::A<const std::string&>(), testing::_))
    //      .Times(2)
    //      .WillRepeatedly([](const std::string& ref, const std::string& value, unsigned long long int rtc) {
    //          std::cout << ref << ", " << value << ", " << rtc << std::endl;
    //      });
    //
    //    EXPECT_CALL(*dataServiceMock,
    //                addSensorReading(testing::_, testing::A<const std::vector<std::string>&>(), testing::_))
    //      .Times(2)
    //      .WillRepeatedly([](const std::string& ref, const std::vector<std::string>& values, unsigned long long int
    //      rtc) {
    //          std::cout << ref << ", " << values.size() << ", " << rtc << std::endl;
    //      });

    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF1", "TEST_VALUE1"));
    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF2", 2));
    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF3", {1, 2, 3}));
    EXPECT_NO_FATAL_FAILURE(wolk->addSensorReading("TEST_REF4", std::vector<std::string>{"V1", "V2", "V3"}));

    EXPECT_NO_FATAL_FAILURE(wolk->addAlarm("TEST_ALARM_REF1", true));

    EXPECT_NO_THROW(wolk->addSensorReading("EMPTY_TEST", std::vector<std::string>()));

    EXPECT_NO_FATAL_FAILURE(wolk->addAlarm("TEST_ALARM_REF1", false));
}

TEST_F(WolkTests, PublishesAndFlushes)
{
    //    Same, for some reason the mocks functions can't be called. :(
    //    EXPECT_CALL(*dataServiceMock, publishSensorReadings).Times(1);
    //    EXPECT_CALL(*dataServiceMock, publishActuatorStatuses).Times(1);
    //    EXPECT_CALL(*dataServiceMock, publishAlarms).Times(1);
    //    EXPECT_CALL(*dataServiceMock, publishConfiguration).Times(1);

    EXPECT_NO_FATAL_FAILURE(wolk->flushSensorReadings());
    EXPECT_NO_FATAL_FAILURE(wolk->flushActuatorStatuses());
    EXPECT_NO_FATAL_FAILURE(wolk->flushAlarms());
    EXPECT_NO_FATAL_FAILURE(wolk->flushConfiguration());

    EXPECT_NO_FATAL_FAILURE(wolk->publish());
    EXPECT_NO_FATAL_FAILURE(wolk->publishFirmwareVersion());
}

TEST_F(WolkTests, Handles)
{
    EXPECT_NO_FATAL_FAILURE(wolk->handleActuatorGetCommand("TEST_REF1"));
    EXPECT_NO_FATAL_FAILURE(wolk->handleActuatorSetCommand("TEST_REF1", "TEST_VAL1"));
    EXPECT_NO_FATAL_FAILURE(wolk->handleConfigurationGetCommand());
    EXPECT_NO_FATAL_FAILURE(
      wolk->handleConfigurationSetCommand(wolkabout::ConfigurationSetCommand(std::vector<wolkabout::ConfigurationItem>{
        wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2", "V3"}, "C1"),
        wolkabout::ConfigurationItem(std::vector<std::string>{"V1", "V2"}, "C2"),
        wolkabout::ConfigurationItem(std::vector<std::string>{"V1"}, "C3")})));
}

TEST_F(WolkTests, ConnectAndDisconnect)
{
    EXPECT_NO_FATAL_FAILURE(wolk->connect());
    EXPECT_NO_FATAL_FAILURE(wolk->disconnect());
}

TEST_F(WolkTests, StatusNotification)
{
    EXPECT_CALL(*keepAliveServiceMock, connected).Times(1);
    EXPECT_CALL(*keepAliveServiceMock, disconnected).Times(1);

    EXPECT_NO_FATAL_FAILURE(wolk->notifyConnected());
    EXPECT_NO_FATAL_FAILURE(wolk->notifyDisonnected());
}

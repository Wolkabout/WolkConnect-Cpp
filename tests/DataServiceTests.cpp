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

using namespace wolkabout::connect;
using namespace ::testing;

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
        _internalFeedUpdateSetHandler = [&](const std::string& deviceKey,
                                            const std::map<std::uint64_t, std::vector<Reading>>& readings) {
            if (feedUpdateSetHandler)
                feedUpdateSetHandler(deviceKey, readings);
        };
        _internalParameterSyncHandler = [&](const std::string& deviceKey, const std::vector<Parameter>& parameters) {
            if (parameterSyncHandler)
                parameterSyncHandler(deviceKey, parameters);
        };

        // Create the service
        service = std::make_shared<DataService>(*dataProtocolMock, *persistenceMock, *connectivityServiceMock,
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

    std::shared_ptr<DataService> service;

    FeedUpdateSetHandler _internalFeedUpdateSetHandler;
    ParameterSyncHandler _internalParameterSyncHandler;
};

TEST_F(DataServiceTests, MakePersistenceKey)
{
    EXPECT_EQ(service->makePersistenceKey(DEVICE_KEY, "T"), DEVICE_KEY + "+T");
}

TEST_F(DataServiceTests, ParsePersistenceKeyInvalid)
{
    auto pair = std::pair<std::string, std::string>{};
    ASSERT_NO_FATAL_FAILURE(pair = service->parsePersistenceKey("AB"));
    EXPECT_TRUE(pair.first.empty());
    EXPECT_TRUE(pair.second.empty());
}

TEST_F(DataServiceTests, ParsePersistenceKey)
{
    auto pair = std::pair<std::string, std::string>{};
    ASSERT_NO_FATAL_FAILURE(pair = service->parsePersistenceKey("A+B"));
    EXPECT_EQ(pair.first, "A");
    EXPECT_EQ(pair.second, "B");
}

TEST_F(DataServiceTests, PublishReadingsFromPersistenceEmptyPersistence)
{
    EXPECT_CALL(*persistenceMock, getReadings).WillOnce(Return(std::vector<std::shared_ptr<Reading>>{}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(A<const std::string&>(), A<FeedValuesMessage>())).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->publishReadingsForPersistenceKey(""));
}

TEST_F(DataServiceTests, PublishReadingsEmptyDeviceKey)
{
    EXPECT_CALL(*persistenceMock, getReadings)
      .WillOnce(Return(std::vector<std::shared_ptr<Reading>>{std::make_shared<Reading>("T", "TestValue", 123456789)}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(A<const std::string&>(), A<FeedValuesMessage>())).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->publishReadingsForPersistenceKey(""));
}

TEST_F(DataServiceTests, PublishReadingsFailsToParse)
{
    EXPECT_CALL(*persistenceMock, getReadings)
      .WillOnce(Return(std::vector<std::shared_ptr<Reading>>{std::make_shared<Reading>("T", "TestValue", 123456789)}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(A<const std::string&>(), A<FeedValuesMessage>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->publishReadingsForPersistenceKey(DEVICE_KEY + "+" + "T"));
}

TEST_F(DataServiceTests, PublishReadingsHappyFlow)
{
    EXPECT_CALL(*persistenceMock, getReadings)
      .WillOnce(Return(std::vector<std::shared_ptr<Reading>>{std::make_shared<Reading>("T", "TestValue", 123456789)}))
      .WillOnce(Return(std::vector<std::shared_ptr<Reading>>{}));
    EXPECT_CALL(*persistenceMock, removeReadings).Times(1);
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(A<const std::string&>(), A<FeedValuesMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->publishReadingsForPersistenceKey(DEVICE_KEY + "+" + "T"));
}

TEST_F(DataServiceTests, CheckIfSubscriptionExistButItsEmpty)
{
    ASSERT_FALSE(
      service->checkIfSubscriptionIsWaiting(std::make_shared<ParametersUpdateMessage>(std::vector<Parameter>{})));
}

TEST_F(DataServiceTests, CheckIfSubscriptionExistTwoSubscription)
{
    // Add the two subscriptions
    service->m_parameterSubscriptions.emplace(
      0, DataService::ParameterSubscription{
           {ParameterName::FIRMWARE_UPDATE_REPOSITORY, ParameterName::FIRMWARE_UPDATE_CHECK_TIME},
           [](const std::vector<Parameter>&) {}});
    service->m_parameterSubscriptions.emplace(
      1, DataService::ParameterSubscription{{ParameterName::FILE_TRANSFER_PLATFORM_ENABLED},
                                            [](const std::vector<Parameter>&) {}});
    std::atomic_bool callbackCalled{false};
    std::mutex mutex;
    std::condition_variable conditionVariable;
    service->m_parameterSubscriptions.emplace(
      2, DataService::ParameterSubscription{{ParameterName::EXTERNAL_ID}, [&](const std::vector<Parameter>&) {
                                                callbackCalled = true;
                                                conditionVariable.notify_one();
                                            }});

    // Now parse the subscription
    ASSERT_TRUE(service->checkIfSubscriptionIsWaiting(
      std::make_shared<ParametersUpdateMessage>(std::vector<Parameter>{{ParameterName::EXTERNAL_ID, "TestValue"}})));
    if (!callbackCalled)
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
    }
    EXPECT_TRUE(callbackCalled);
}

TEST_F(DataServiceTests, AddReadingSingleStringReading)
{
    EXPECT_CALL(*persistenceMock, putReading).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->addReading(DEVICE_KEY, "T", "Value", 1234567890));
}

TEST_F(DataServiceTests, AddReadingMultiStringReading)
{
    EXPECT_CALL(*persistenceMock, putReading).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->addReading(DEVICE_KEY, "T", {"Value1", "Value2", "Value3"}, 1234567890));
}

TEST_F(DataServiceTests, AddReading)
{
    EXPECT_CALL(*persistenceMock, putReading).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->addReading(DEVICE_KEY, Reading{"T", std::string{"Value"}, 1234567890}));
}

TEST_F(DataServiceTests, AddReadings)
{
    EXPECT_CALL(*persistenceMock, putReading).Times(3);
    ASSERT_NO_FATAL_FAILURE(
      service->addReadings(DEVICE_KEY, std::vector<Reading>{{"T", std::uint64_t{123}, 1234567890},
                                                            {"T", std::uint64_t{456}, 1234567891},
                                                            {"T", std::uint64_t{789}, 1234567892}}));
}

TEST_F(DataServiceTests, AddAttribute)
{
    EXPECT_CALL(*persistenceMock, putAttribute).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->addAttribute(DEVICE_KEY, Attribute{"T", DataType::STRING, "TestValue"}));
}

TEST_F(DataServiceTests, UpdateParameter)
{
    EXPECT_CALL(*persistenceMock, putParameter).Times(1);
    ASSERT_NO_FATAL_FAILURE(
      service->updateParameter(DEVICE_KEY, Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}));
}

TEST_F(DataServiceTests, RegisterSingleFeedTest)
{
    auto feed = Feed{"Test Feed", "T", FeedType::IN_OUT, Unit::AMPERE};
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRegistrationMessage>()))
      .WillOnce([&](const std::string&, const FeedRegistrationMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->registerFeed(DEVICE_KEY, feed));
}

TEST_F(DataServiceTests, RegisterSingleFeedTestFailsToPublish)
{
    auto feed = Feed{"Test Feed", "T", FeedType::IN_OUT, Unit::AMPERE};
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRegistrationMessage>()))
      .WillOnce([&](const std::string&, const FeedRegistrationMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->registerFeed(DEVICE_KEY, feed));
}

TEST_F(DataServiceTests, RegisterSingleFeedTestFailsToParse)
{
    auto feed = Feed{"Test Feed", "T", FeedType::IN_OUT, Unit::AMPERE};
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRegistrationMessage>()))
      .WillOnce([&](const std::string&, const FeedRegistrationMessage&) { return nullptr; });
    EXPECT_CALL(*connectivityServiceMock, publish).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->registerFeed(DEVICE_KEY, feed));
}

TEST_F(DataServiceTests, RemoveSingleFeedTest)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRemovalMessage>()))
      .WillOnce([&](const std::string&, const FeedRemovalMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->removeFeed(DEVICE_KEY, "TestFeed"));
}

TEST_F(DataServiceTests, RemoveSingleFeedTestFailsToPublish)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRemovalMessage>()))
      .WillOnce([&](const std::string&, const FeedRemovalMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->removeFeed(DEVICE_KEY, "TestFeed"));
}

TEST_F(DataServiceTests, RemoveSingleFeedTestFailsToParse)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<FeedRemovalMessage>()))
      .WillOnce([&](const std::string&, const FeedRemovalMessage&) { return nullptr; });
    EXPECT_CALL(*connectivityServiceMock, publish).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->removeFeed(DEVICE_KEY, "TestFeed"));
}

TEST_F(DataServiceTests, PullFeedTest)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<PullFeedValuesMessage>()))
      .WillOnce([&](const std::string&, const PullFeedValuesMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->pullFeedValues(DEVICE_KEY));
}

TEST_F(DataServiceTests, PullFeedTestFailsToPublish)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<PullFeedValuesMessage>()))
      .WillOnce([&](const std::string&, const PullFeedValuesMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->pullFeedValues(DEVICE_KEY));
}

TEST_F(DataServiceTests, PullFeedTestFailsToParse)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<PullFeedValuesMessage>()))
      .WillOnce([&](const std::string&, const PullFeedValuesMessage&) { return nullptr; });
    EXPECT_CALL(*connectivityServiceMock, publish).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->pullFeedValues(DEVICE_KEY));
}

TEST_F(DataServiceTests, PullParameterTest)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersPullMessage>()))
      .WillOnce([&](const std::string&, const ParametersPullMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->pullParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, PullParameterTestFailsToPublish)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersPullMessage>()))
      .WillOnce([&](const std::string&, const ParametersPullMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->pullParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, PullParameterTestFailsToParse)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersPullMessage>()))
      .WillOnce([&](const std::string&, const ParametersPullMessage&) { return nullptr; });
    EXPECT_CALL(*connectivityServiceMock, publish).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->pullParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, SynchronizeParametersTest)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<SynchronizeParametersMessage>()))
      .WillOnce([&](const std::string&, const SynchronizeParametersMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->synchronizeParameters(DEVICE_KEY, {}, [](const std::vector<Parameter>&) {}));
}

TEST_F(DataServiceTests, SynchronizeParametersTestFailsToPublish)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<SynchronizeParametersMessage>()))
      .WillOnce([&](const std::string&, const SynchronizeParametersMessage&) {
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->synchronizeParameters(DEVICE_KEY, {}, [](const std::vector<Parameter>&) {}));
}

TEST_F(DataServiceTests, SynchronizeParametersTestFailsToParse)
{
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<SynchronizeParametersMessage>()))
      .WillOnce([&](const std::string&, const SynchronizeParametersMessage&) { return nullptr; });
    EXPECT_CALL(*connectivityServiceMock, publish).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->synchronizeParameters(DEVICE_KEY, {}, [](const std::vector<Parameter>&) {}));
}

TEST_F(DataServiceTests, PublishReadings)
{
    EXPECT_CALL(*persistenceMock, getReadingsKeys).WillOnce(Return(std::vector<std::string>{DEVICE_KEY}));
    ASSERT_NO_FATAL_FAILURE(service->publishReadings());
    ASSERT_NO_FATAL_FAILURE(service->publishReadings(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishAttributesNoAttributes)
{
    EXPECT_CALL(*persistenceMock, getAttributes).WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>()));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes());
}

TEST_F(DataServiceTests, PublishAttributesFailsToParse)
{
    EXPECT_CALL(*persistenceMock, getAttributes)
      .WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>{
        {DEVICE_KEY + "+" + "T", std::make_shared<Attribute>("T", DataType::STRING, "TestValue")}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<AttributeRegistrationMessage>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes());
}

TEST_F(DataServiceTests, PublishAttributesFailsToPublish)
{
    EXPECT_CALL(*persistenceMock, getAttributes)
      .WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>{
        {DEVICE_KEY + "+" + "T", std::make_shared<Attribute>("T", DataType::STRING, "TestValue")}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<AttributeRegistrationMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes());
}

TEST_F(DataServiceTests, PublishAttributesHappyFlow)
{
    EXPECT_CALL(*persistenceMock, getAttributes)
      .WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>{
        {DEVICE_KEY + "+" + "T", std::make_shared<Attribute>("T", DataType::STRING, "TestValue")}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<AttributeRegistrationMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes());
}

TEST_F(DataServiceTests, PublishAttributesForDeviceNoAttributes)
{
    EXPECT_CALL(*persistenceMock, getAttributes).WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>()));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishAttributesForDeviceFailsToParse)
{
    EXPECT_CALL(*persistenceMock, getAttributes)
      .WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>{
        {DEVICE_KEY + "+" + "T", std::make_shared<Attribute>("T", DataType::STRING, "TestValue")}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<AttributeRegistrationMessage>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishAttributesForDeviceFailsToPublish)
{
    EXPECT_CALL(*persistenceMock, getAttributes)
      .WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>{
        {DEVICE_KEY + "+" + "T", std::make_shared<Attribute>("T", DataType::STRING, "TestValue")}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<AttributeRegistrationMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishAttributesForDeviceHappyFlow)
{
    EXPECT_CALL(*persistenceMock, getAttributes)
      .WillOnce(Return(std::map<std::string, std::shared_ptr<Attribute>>{
        {DEVICE_KEY + "+" + "T", std::make_shared<Attribute>("T", DataType::STRING, "TestValue")}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<AttributeRegistrationMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->publishAttributes(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishParametersNoAttributes)
{
    EXPECT_CALL(*persistenceMock, getParameters).WillOnce(Return(std::map<std::string, Parameter>()));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters());
}

TEST_F(DataServiceTests, PublishParametersFailsToParse)
{
    EXPECT_CALL(*persistenceMock, getParameters)
      .WillOnce(Return(std::map<std::string, Parameter>{
        {DEVICE_KEY + "+" + "T", Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersUpdateMessage>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters());
}

TEST_F(DataServiceTests, PublishParametersFailsToPublish)
{
    EXPECT_CALL(*persistenceMock, getParameters)
      .WillOnce(Return(std::map<std::string, Parameter>{
        {DEVICE_KEY + "+" + "T", Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersUpdateMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters());
}

TEST_F(DataServiceTests, PublishParametersHappyFlow)
{
    EXPECT_CALL(*persistenceMock, getParameters)
      .WillOnce(Return(std::map<std::string, Parameter>{
        {DEVICE_KEY + "+" + "T", Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersUpdateMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters());
}

TEST_F(DataServiceTests, PublishParametersForDeviceNoAttributes)
{
    EXPECT_CALL(*persistenceMock, getParameters).WillOnce(Return(std::map<std::string, Parameter>()));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishParametersForDeviceFailsToParse)
{
    EXPECT_CALL(*persistenceMock, getParameters)
      .WillOnce(Return(std::map<std::string, Parameter>{
        {DEVICE_KEY + "+" + "T", Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersUpdateMessage>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishParametersForDeviceFailsToPublish)
{
    EXPECT_CALL(*persistenceMock, getParameters)
      .WillOnce(Return(std::map<std::string, Parameter>{
        {DEVICE_KEY + "+" + "T", Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersUpdateMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(false));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, PublishParametersForDeviceHappyFlow)
{
    EXPECT_CALL(*persistenceMock, getParameters)
      .WillOnce(Return(std::map<std::string, Parameter>{
        {DEVICE_KEY + "+" + "T", Parameter{ParameterName::EXTERNAL_ID, "TestExternalId"}}}));
    EXPECT_CALL(*dataProtocolMock, makeOutboundMessage(_, A<ParametersUpdateMessage>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish).WillOnce(Return(true));
    ASSERT_NO_FATAL_FAILURE(service->publishParameters(DEVICE_KEY));
}

TEST_F(DataServiceTests, MessageReceivedMessageIsNull)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(nullptr));
}

TEST_F(DataServiceTests, MessageReceivedMessageEmptyDeviceKey)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(""));
    EXPECT_CALL(*dataProtocolMock, getMessageType).Times(0);
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(DataServiceTests, MessageReceivedMessageInvalidMessageType)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(*dataProtocolMock, getMessageType).WillOnce(Return(MessageType::UNKNOWN));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(DataServiceTests, MessageReceivedMessageFeedFailedToParse)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(*dataProtocolMock, getMessageType).WillOnce(Return(MessageType::FEED_VALUES));
    EXPECT_CALL(*dataProtocolMock, parseFeedValues).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(DataServiceTests, MessageReceivedMessageFeedHappyFlow)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(*dataProtocolMock, getMessageType).WillOnce(Return(MessageType::FEED_VALUES));
    EXPECT_CALL(*dataProtocolMock, parseFeedValues)
      .WillOnce(Return(ByMove(std::unique_ptr<FeedValuesMessage>{
        new FeedValuesMessage{std::vector<Reading>{Reading{"T", std::uint64_t{123}, 1234567890}}}})));

    // Set up the callback
    std::atomic_bool callbackCalled{false};
    std::mutex mutex;
    std::condition_variable conditionVariable;
    feedUpdateSetHandler = [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>& readings) {
        if (!readings.empty())
        {
            callbackCalled = true;
            conditionVariable.notify_one();
        }
    };

    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
    if (!callbackCalled)
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
    }
    EXPECT_TRUE(callbackCalled);
}

TEST_F(DataServiceTests, MessageReceivedMessageParameterFailedToParse)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(*dataProtocolMock, getMessageType).WillOnce(Return(MessageType::PARAMETER_SYNC));
    EXPECT_CALL(*dataProtocolMock, parseParameters).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(DataServiceTests, MessageReceivedMessageParameterHappyFlow)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(*dataProtocolMock, getMessageType).WillOnce(Return(MessageType::PARAMETER_SYNC));
    EXPECT_CALL(*dataProtocolMock, parseParameters)
      .WillOnce(Return(ByMove(std::unique_ptr<ParametersUpdateMessage>{
        new ParametersUpdateMessage{std::vector<Parameter>{{ParameterName::EXTERNAL_ID, "TestExternalId"}}}})));

    // Set up the callback
    std::atomic_bool callbackCalled{false};
    std::mutex mutex;
    std::condition_variable conditionVariable;
    parameterSyncHandler = [&](const std::string&, const std::vector<Parameter>& parameters) {
        if (!parameters.empty())
        {
            callbackCalled = true;
            conditionVariable.notify_one();
        }
    };

    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
    if (!callbackCalled)
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
    }
    EXPECT_TRUE(callbackCalled);
}

TEST_F(DataServiceTests, MessageReceivedMessageParameterHappyFlowAnswersSubscription)
{
    EXPECT_CALL(*dataProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(*dataProtocolMock, getMessageType).WillOnce(Return(MessageType::PARAMETER_SYNC));
    EXPECT_CALL(*dataProtocolMock, parseParameters)
      .WillOnce(Return(ByMove(std::unique_ptr<ParametersUpdateMessage>{
        new ParametersUpdateMessage{std::vector<Parameter>{{ParameterName::EXTERNAL_ID, "TestExternalId"}}}})));

    // Set up the callback
    std::atomic_bool callbackCalled{false};
    std::mutex mutex;
    std::condition_variable conditionVariable;
    service->m_parameterSubscriptions.emplace(
      0,
      DataService::ParameterSubscription{{ParameterName::EXTERNAL_ID}, [&](const std::vector<Parameter>& parameters) {
                                             if (!parameters.empty())
                                             {
                                                 callbackCalled = true;
                                                 conditionVariable.notify_one();
                                             }
                                         }});

    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
    if (!callbackCalled)
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
    }
    EXPECT_TRUE(callbackCalled);
}

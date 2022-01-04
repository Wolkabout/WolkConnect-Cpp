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
#include "wolk/Wolk.h"
#include "wolk/WolkBuilder.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/FeedUpdateHandlerMock.h"
#include "mocks/ParameterHandlerMock.h"
#include "mocks/PersistenceMock.h"

#include <gmock/gmock.h>

using namespace wolkabout;
using namespace ::testing;

class WolkBuilderTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    const std::string TAG = "WolkBuilderTests";
};

TEST_F(WolkBuilderTests, CtorTests)
{
    EXPECT_NO_THROW(WolkBuilder(Device("TEST_KEY", "TEST_PASSWORD", OutboundDataMode::PUSH)));
}

TEST_F(WolkBuilderTests, LambdaHandlers)
{
    const auto testDevice = std::make_shared<Device>("TEST_KEY", "TEST_PASSWORD", OutboundDataMode::PUSH);

    // Bool flags
    bool feedUpdated = false;
    bool parameterUpdated = false;

    std::shared_ptr<WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<WolkBuilder>(*testDevice));

    ASSERT_NO_THROW(builder->feedUpdateHandler(
      [&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) { feedUpdated = true; }));
    ASSERT_NO_THROW(
      builder->parameterHandler([&](const std::string&, const std::vector<Parameter>&) { parameterUpdated = true; }));

    std::shared_ptr<Wolk> wolk = nullptr;
    EXPECT_NO_THROW(wolk = builder->build());

    wolk->handleFeedUpdateCommand("", {});
    wolk->handleParameterCommand("", {});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(feedUpdated);
    EXPECT_TRUE(parameterUpdated);
}

TEST_F(WolkBuilderTests, MockHandlers)
{
    const auto testDevice = std::make_shared<Device>("TEST_KEY", "TEST_PASSWORD", OutboundDataMode::PUSH);

    // Bool flags
    bool feedUpdated = false;
    bool parameterUpdated = false;

    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    auto feedUpdateHandler = std::make_shared<NiceMock<FeedUpdateHandlerMock>>();
    ASSERT_NO_THROW(builder->feedUpdateHandler(feedUpdateHandler));
    EXPECT_CALL(*feedUpdateHandler, handleUpdate)
      .WillOnce([&](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) { feedUpdated = true; });

    auto parameterHandler = std::make_shared<NiceMock<ParameterHandlerMock>>();
    ASSERT_NO_THROW(builder->parameterHandler(parameterHandler));
    EXPECT_CALL(*parameterHandler, handleUpdate).WillOnce([&](const std::string&, const std::vector<Parameter>&) {
        parameterUpdated = true;
    });

    std::shared_ptr<Wolk> wolk = nullptr;
    EXPECT_NO_THROW(wolk = builder->build());

    wolk->handleFeedUpdateCommand("", {});
    wolk->handleParameterCommand("", {});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(feedUpdated);
    EXPECT_TRUE(parameterUpdated);
}

TEST_F(WolkBuilderTests, OtherProperties)
{
    const auto testDevice = std::make_shared<Device>("TEST_KEY", "TEST_PASSWORD", OutboundDataMode::PUSH);
    std::shared_ptr<wolkabout::WolkBuilder> builder;
    ASSERT_NO_THROW(builder = std::make_shared<wolkabout::WolkBuilder>(*testDevice));

    EXPECT_NO_THROW(builder->host("some_other_host"));
    EXPECT_NO_THROW(builder->caCertPath("some_ca_cert_path"));
    EXPECT_NO_THROW(builder->withDataProtocol(std::unique_ptr<DataProtocolMock>(new DataProtocolMock)));
    EXPECT_NO_THROW(builder->withPersistence(std::make_shared<PersistenceMock>()));
}

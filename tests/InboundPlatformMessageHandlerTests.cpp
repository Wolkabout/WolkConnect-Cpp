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
#ifndef WOLKABOUTCONNECTOR_INBOUNDPLATFORMMESSAGEHANDLERTESTS_CPP
#define WOLKABOUTCONNECTOR_INBOUNDPLATFORMMESSAGEHANDLERTESTS_CPP

#define private public
#define protected public
#include "InboundPlatformMessageHandler.h"
#undef private
#undef protected

#include "mocks/MessageListenerMock.h"

#include <gtest/gtest.h>
#include <iostream>

class InboundPlatformMessageHandlerTests : public ::testing::Test
{
};

TEST_F(InboundPlatformMessageHandlerTests, SimpleTest)
{
    const auto& messageListener = std::make_shared<MessageListenerMock>();
    const auto& messageHandler = std::make_shared<wolkabout::InboundPlatformMessageHandler>("TEST_DEVICE");

    messageHandler->addListener(messageListener);
}

#endif    // WOLKABOUTCONNECTOR_INBOUNDPLATFORMMESSAGEHANDLERTESTS_CPP

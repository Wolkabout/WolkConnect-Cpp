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
#ifndef WOLKABOUTCONNECTOR_MESSAGELISTENERMOKC_H
#define WOLKABOUTCONNECTOR_MESSAGELISTENERMOKC_H

#include "InboundPlatformMessageHandler.h"

#include "mocks/ProtocolMock.h"

#include <gmock/gmock.h>

class MessageListenerMock : public wolkabout::MessageListener
{
public:
    explicit MessageListenerMock(wolkabout::Protocol& protocol): m_protocol(protocol) {}

    void messageReceived(std::shared_ptr<wolkabout::Message> message) override
    {
        if (m_handler)
        {
            m_handler(message);
        }
    }

    const wolkabout::Protocol& getProtocol() override { return m_protocol; }

    void setHandler(const std::function<void(std::shared_ptr<wolkabout::Message>)>& handler) { m_handler = handler; }

private:
    std::function<void(std::shared_ptr<wolkabout::Message>)> m_handler;

    wolkabout::Protocol& m_protocol;
};

#endif    // WOLKABOUTCONNECTOR_MESSAGELISTENERMOKC_H

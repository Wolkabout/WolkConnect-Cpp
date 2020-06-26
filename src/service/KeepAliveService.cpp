/*
 * Copyright 2018 WolkAbout Technology s.r.o.
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

#include "service/KeepAliveService.h"
#include "connectivity/ConnectivityService.h"
#include "model/Message.h"
#include "protocol/StatusProtocol.h"
#include "utilities/Logger.h"
#include "utilities/json.hpp"

namespace wolkabout
{
KeepAliveService::KeepAliveService(std::string deviceKey, StatusProtocol& protocol,
                                   ConnectivityService& connectivityService, std::chrono::seconds keepAliveInterval)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_connectivityService{connectivityService}
, m_keepAliveInterval{std::move(keepAliveInterval)}
{
    std::shared_ptr<Message> lastWillMessage = m_protocol.makeLastWillMessage({m_deviceKey});
    if (lastWillMessage)
    {
        m_connectivityService.setUncontrolledDisonnectMessage(lastWillMessage);
    }
}

void KeepAliveService::connected()
{
    auto ping = [=] {
        std::shared_ptr<Message> message = m_protocol.makeFromPingRequest(m_deviceKey);

        if (message)
        {
            m_connectivityService.publish(message);
        }
    };

    // send as soon as connected
    ping();

    m_timer.run(std::chrono::duration_cast<std::chrono::milliseconds>(m_keepAliveInterval), ping);
}

void KeepAliveService::disconnected()
{
    m_timer.stop();
}

long long int KeepAliveService::getLastTimestamp() const
{
    return m_lastTimestamp;
}

void KeepAliveService::messageReceived(std::shared_ptr<Message> message)
{
    if (m_protocol.isPongMessage(*message))
    {
        const auto& payload = nlohmann::json::parse(message->getContent());
        m_lastTimestamp = payload["value"];
        LOG(DEBUG) << "KeepAliveService: Timestamp received: " << m_lastTimestamp;
    }
}

const Protocol& KeepAliveService::getProtocol()
{
    return m_protocol;
}
}    // namespace wolkabout

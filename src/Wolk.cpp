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

#include "Wolk.h"

#include "core/connectivity/ConnectivityService.h"
#include "core/model/Device.h"
#include "core/model/Message.h"
#include "core/protocol/DataProtocol.h"
#include "service/data/DataService.h"
#include "core/utilities/Logger.h"
#include "api/ParameterHandler.h"

#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace wolkabout
{

Wolk::~Wolk() = default;

WolkBuilder Wolk::newBuilder(Device device)
{
    return WolkBuilder(device);
}

void Wolk::addReading(const std::string& reference, std::string value, unsigned long long rtc)
{
    if (rtc == 0)
    {
        rtc = Wolk::currentRtc();
    }

    addToCommandBuffer([=]() -> void { m_dataService->addReading(reference, value, rtc); });
}

void Wolk::connect()
{
    tryConnect(true);
}

void Wolk::disconnect()
{
    addToCommandBuffer([=]() -> void {
        m_connectivityService->disconnect();
        notifyDisonnected();
    });
}

void Wolk::publish()
{
    addToCommandBuffer([=]() -> void {
        flushAttributes();
        flushReadings();
        flushParameters();
    });
}

Wolk::Wolk(Device device)
: m_device(device)
{
    m_commandBuffer = std::unique_ptr<CommandBuffer>(new CommandBuffer());
}

void Wolk::addToCommandBuffer(std::function<void()> command)
{
    m_commandBuffer->pushCommand(std::make_shared<std::function<void()>>(command));
}

unsigned long long Wolk::currentRtc()
{
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    return static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

void Wolk::flushAttributes()
{
    m_dataService->publishAttributes();
}

void Wolk::flushReadings()
{
    m_dataService->publishReadings();
}

void Wolk::flushParameters()
{
    m_dataService->publishParameters();
}

void Wolk::handleFeedUpdateCommand(const std::map<unsigned long long int, std::vector<Reading>> readings)
{
    LOG(INFO) << "Received feed update";

    addToCommandBuffer([=] {
        if (auto provider = m_feedUpdateHandler.lock())
        {
            provider->handleUpdate(readings);
        }
        else if (m_feedUpdateHandlerLambda)
        {
            m_feedUpdateHandlerLambda(readings);
        }
    });
}

void Wolk::handleParameterCommand(const std::vector<Parameters> parameters)
{
    LOG(INFO) << "Received parameter sync";

    addToCommandBuffer([=] {
                           if (auto provider = m_parameterHandler.lock())
                           {
                               provider->handleUpdate(parameters);
                           }
                           else if (m_parameterLambda)
                           {
                               m_parameterLambda(parameters);
                           }
                       });
}

void Wolk::tryConnect(bool firstTime)
{
    addToCommandBuffer([=]() -> void {
        if (firstTime)
            LOG(INFO) << "Connecting...";

        if (!m_connectivityService->connect())
        {
            if (firstTime)
                LOG(INFO) << "Failed to connect";

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            tryConnect(false);
            return;
        }

        notifyConnected();
    });
}

void Wolk::notifyConnected()
{
    LOG(INFO) << "Connection established";

    publish();
}

void Wolk::notifyDisonnected()
{
    LOG(INFO) << "Connection lost";
}
void Wolk::registerFeed(Feed feed)
{
    addToCommandBuffer([=]() -> void { m_dataService->registerFeed(feed); });
}
void Wolk::pullFeedValues()
{
    addToCommandBuffer([=]() -> void { m_dataService->pullFeedValues(); });
}

void Wolk::pullParameters()
{
    addToCommandBuffer([=]() -> void { m_dataService->pullParameters(); });
}
void Wolk::addAttribute(Attribute attribute)
{
    addToCommandBuffer([=]() -> void { m_dataService->addAttribute(attribute); });
}
void Wolk::updateParameter(Parameters parameter)
{
    addToCommandBuffer([=]() -> void { m_dataService->updateParameter(parameter); });
}

Wolk::ConnectivityFacade::ConnectivityFacade(InboundMessageHandler& handler,
                                             std::function<void()> connectionLostHandler)
: m_messageHandler{handler}, m_connectionLostHandler{connectionLostHandler}
{
}

void Wolk::ConnectivityFacade::messageReceived(const std::string& channel, const std::string& message)
{
    m_messageHandler.messageReceived(channel, message);
}

void Wolk::ConnectivityFacade::connectionLost()
{
    m_connectionLostHandler();
}

std::vector<std::string> Wolk::ConnectivityFacade::getChannels() const
{
    return m_messageHandler.getChannels();
}
}    // namespace wolkabout

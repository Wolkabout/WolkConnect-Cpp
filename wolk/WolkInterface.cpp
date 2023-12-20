/**
 * Copyright 2022 Wolkabout Technology s.r.o.
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

#include "wolk/WolkInterface.h"

#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/protocol/FileManagementProtocol.h"
#include "core/protocol/FirmwareUpdateProtocol.h"
#include "core/protocol/PlatformStatusProtocol.h"
#include "core/protocol/RegistrationProtocol.h"
#include "core/utility/Logger.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/file_management/FileManagementService.h"
#include "wolk/service/firmware_update/FirmwareUpdateService.h"
#include "wolk/service/platform_status/PlatformStatusService.h"
#include "wolk/service/registration_service/RegistrationService.h"

using namespace wolkabout::legacy;

namespace wolkabout
{
namespace connect
{
WolkInterface::~WolkInterface() = default;

void WolkInterface::connect()
{
    tryConnect(true);
}

void WolkInterface::disconnect()
{
    addToCommandBuffer([=]() -> void {
        m_connectivityService->disconnect();
        notifyDisconnected();
    });
}

bool WolkInterface::isConnected()
{
    return m_connected;
}

void WolkInterface::setConnectionStatusListener(ConnectionStatusListener listener)
{
    m_connectionStatusListener = listener;
}

void WolkInterface::publish()
{
    addToCommandBuffer([=]() -> void {
        flushAttributes();
        flushReadings();
        flushParameters();
    });
}

WolkInterface::WolkInterface() : m_connected(false), m_commandBuffer(new legacy::CommandBuffer) {}

void WolkInterface::tryConnect(bool firstTime)
{
    addToCommandBuffer([=]() -> void {
        if (firstTime)
            LOG(INFO) << "Connecting...";

        if (!m_connectivityService->connect())
        {
            if (firstTime)
                LOG(INFO) << "Failed to connect";

			std::this_thread::sleep_for(std::chrono::milliseconds(20000));
            tryConnect(false);
            return;
        }

        notifyConnected();
    });
}

void WolkInterface::notifyConnected()
{
    LOG(INFO) << "Connection established";

    m_connected = true;

    if (m_registrationService != nullptr)
        m_registrationService->start();

    notifyConnectionStatusListener();
    publish();
}

void WolkInterface::notifyDisconnected()
{
    LOG(INFO) << "Connection lost";

    m_connected = false;
    notifyConnectionStatusListener();
}

void WolkInterface::notifyConnectionStatusListener()
{
    if (m_connectionStatusListener)
    {
        m_commandBuffer->pushCommand(std::make_shared<std::function<void()>>([this]() {
            if (m_connectionStatusListener)
                m_connectionStatusListener(m_connected);
        }));
    }
}

void WolkInterface::flushAttributes()
{
    m_dataService->publishAttributes();
}

void WolkInterface::flushReadings()
{
    m_dataService->publishReadings();
}

void WolkInterface::flushParameters()
{
    m_dataService->publishParameters();
}

void WolkInterface::handleFeedUpdateCommand(const std::string& deviceKey,
                                            const std::map<std::uint64_t, std::vector<Reading>>& readings)
{
    LOG(INFO) << "Received feed update";

    addToCommandBuffer([=] {
        if (auto provider = m_feedUpdateHandler.lock())
        {
            provider->handleUpdate(deviceKey, readings);
        }
        else if (m_feedUpdateHandlerLambda)
        {
            m_feedUpdateHandlerLambda(deviceKey, readings);
        }
    });
}

void WolkInterface::handleParameterCommand(const std::string& deviceKey, const std::vector<Parameter>& parameters)
{
    LOG(INFO) << "Received parameter sync";

    addToCommandBuffer([=] {
        if (auto provider = m_parameterHandler.lock())
        {
            provider->handleUpdate(deviceKey, parameters);
        }
        else if (m_parameterLambda)
        {
            m_parameterLambda(deviceKey, parameters);
        }
    });
}

std::uint64_t WolkInterface::currentRtc()
{
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

void WolkInterface::addToCommandBuffer(std::function<void()> command)
{
    m_commandBuffer->pushCommand(std::make_shared<std::function<void()>>(command));
}
}    // namespace connect
}    // namespace wolkabout

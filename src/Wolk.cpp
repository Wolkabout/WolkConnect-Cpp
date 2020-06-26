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
#include "ActuationHandler.h"
#include "ActuatorStatusProvider.h"
#include "WolkBuilder.h"
#include "connectivity/ConnectivityService.h"
#include "model/ActuatorStatus.h"
#include "model/Alarm.h"
#include "model/ConfigurationItem.h"
#include "model/ConfigurationSetCommand.h"
#include "model/Device.h"
#include "model/Message.h"
#include "model/SensorReading.h"
#include "service/DataService.h"
#include "service/FirmwareUpdateService.h"
#include "service/KeepAliveService.h"
#include "utilities/Logger.h"

#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace wolkabout
{
const constexpr std::chrono::seconds Wolk::KEEP_ALIVE_INTERVAL;

WolkBuilder Wolk::newBuilder(Device device)
{
    return WolkBuilder(device);
}

void Wolk::addSensorReading(const std::string& reference, std::string value, unsigned long long rtc)
{
    if (rtc == 0)
    {
        rtc = Wolk::currentRtc();
    }

    addToCommandBuffer([=]() -> void { m_dataService->addSensorReading(reference, value, rtc); });
}

void Wolk::addSensorReading(const std::string& reference, const std::vector<std::string> values,
                            unsigned long long int rtc)
{
    if (values.empty())
    {
        return;
    }

    if (rtc == 0)
    {
        rtc = Wolk::currentRtc();
    }

    addToCommandBuffer([=]() -> void { m_dataService->addSensorReading(reference, values, rtc); });
}

void Wolk::addAlarm(const std::string& reference, bool active, unsigned long long rtc)
{
    if (rtc == 0)
    {
        rtc = Wolk::currentRtc();
    }

    addToCommandBuffer([=]() -> void { m_dataService->addAlarm(reference, active, rtc); });
}

void Wolk::publishActuatorStatus(const std::string& reference)
{
    addToCommandBuffer([=]() -> void {
        const ActuatorStatus actuatorStatus = [&]() -> ActuatorStatus {
            if (auto provider = m_actuatorStatusProvider.lock())
            {
                return provider->getActuatorStatus(reference);
            }
            else if (m_actuatorStatusProviderLambda)
            {
                return m_actuatorStatusProviderLambda(reference);
            }

            return ActuatorStatus();
        }();

        m_dataService->addActuatorStatus(reference, actuatorStatus.getValue(), actuatorStatus.getState());
        flushActuatorStatuses();
    });
}

void Wolk::publishConfiguration()
{
    addToCommandBuffer([=]() -> void {
        const auto configuration = [=]() -> std::vector<ConfigurationItem> {
            if (auto provider = m_configurationProvider.lock())
            {
                return provider->getConfiguration();
            }
            else if (m_configurationProviderLambda)
            {
                return m_configurationProviderLambda();
            }

            return std::vector<ConfigurationItem>();
        }();

        m_dataService->addConfiguration(configuration);
        flushConfiguration();
    });
}

long long Wolk::getLastTimestamp()
{
    if (m_keepAliveService)
    {
        return m_keepAliveService->getLastTimestamp();
    }
    return 0;
}

void Wolk::connect()
{
    addToCommandBuffer([=]() -> void {
        if (!m_connectivityService->connect())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            connect();
            return;
        }

        notifyConnected();

        publishFirmwareVersion();
        if (m_firmwareUpdateService)
        {
            m_firmwareUpdateService->reportFirmwareUpdateResult();
        }

        for (const std::string& actuatorReference : m_device.getActuatorReferences())
        {
            publishActuatorStatus(actuatorReference);
        }

        publishConfiguration();

        publish();
    });
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
        flushActuatorStatuses();
        flushAlarms();
        flushSensorReadings();
        flushConfiguration();
    });
}

Wolk::Wolk(Device device)
: m_device(device)
, m_actuationHandlerLambda(nullptr)
, m_actuatorStatusProviderLambda(nullptr)
, m_configurationHandlerLambda(nullptr)
, m_configurationProviderLambda(nullptr)
, m_fileRepository(nullptr)
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

void Wolk::flushActuatorStatuses()
{
    m_dataService->publishActuatorStatuses();
}

void Wolk::flushAlarms()
{
    m_dataService->publishAlarms();
}

void Wolk::flushSensorReadings()
{
    m_dataService->publishSensorReadings();
}

void Wolk::flushConfiguration()
{
    m_dataService->publishConfiguration();
}

void Wolk::handleActuatorSetCommand(const std::string& reference, const std::string& value)
{
    addToCommandBuffer([=] {
        if (auto provider = m_actuationHandler.lock())
        {
            provider->handleActuation(reference, value);
        }
        else if (m_actuationHandlerLambda)
        {
            m_actuationHandlerLambda(reference, value);
        }
    });

    publishActuatorStatus(reference);
}

void Wolk::handleActuatorGetCommand(const std::string& reference)
{
    publishActuatorStatus(reference);
}

void Wolk::handleConfigurationSetCommand(const ConfigurationSetCommand& command)
{
    addToCommandBuffer([=]() -> void {
        if (auto handler = m_configurationHandler.lock())
        {
            handler->handleConfiguration(command.getValues());
        }
        else if (m_configurationHandlerLambda)
        {
            m_configurationHandlerLambda(command.getValues());
        }
    });

    publishConfiguration();
}

void Wolk::handleConfigurationGetCommand()
{
    publishConfiguration();
}

void Wolk::publishFirmwareVersion()
{
    if (m_firmwareUpdateService)
    {
        m_firmwareUpdateService->publishFirmwareVersion();
    }
}

void Wolk::notifyConnected()
{
    if (m_keepAliveService)
    {
        m_keepAliveService->connected();
    }
}

void Wolk::notifyDisonnected()
{
    if (m_keepAliveService)
    {
        m_keepAliveService->disconnected();
    }
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

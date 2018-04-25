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
#include "model/ActuatorCommand.h"
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
#include "utilities/StringUtils.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#define INSTANTIATE_ADD_SENSOR_READING_FOR(x)                                                               \
    template void Wolk::addSensorReading<x>(const std::string& reference, x value, unsigned long long rtc); \
    template void Wolk::addSensorReading<x>(const std::string& reference, std::initializer_list<x> value,   \
                                            unsigned long long int rtc);                                    \
    template void Wolk::addSensorReading<x>(const std::string& reference, const std::vector<x> values,      \
                                            unsigned long long int rtc)

namespace wolkabout
{
const constexpr std::chrono::seconds Wolk::KEEP_ALIVE_INTERVAL;

WolkBuilder Wolk::newBuilder(Device device)
{
    return WolkBuilder(device);
}

template <typename T> void Wolk::addSensorReading(const std::string& reference, T value, unsigned long long rtc)
{
    addSensorReading(reference, StringUtils::toString(value), rtc);
}

template <> void Wolk::addSensorReading(const std::string& reference, std::string value, unsigned long long rtc)
{
    addToCommandBuffer(
      [=]() -> void { m_dataService->addSensorReading(reference, value, rtc != 0 ? rtc : Wolk::currentRtc()); });
}

template <typename T>
void Wolk::addSensorReading(const std::string& reference, std::initializer_list<T> values, unsigned long long int rtc)
{
    addSensorReading(reference, std::vector<T>(values), rtc);
}

template <typename T>
void Wolk::addSensorReading(const std::string& reference, const std::vector<T> values, unsigned long long int rtc)
{
    if (values.empty())
    {
        return;
    }

    std::vector<std::string> stringifiedValues(values.size());
    std::transform(values.begin(), values.end(), stringifiedValues.begin(),
                   [&](const T& value) -> std::string { return StringUtils::toString(value); });

    addToCommandBuffer([=]() -> void {
        m_dataService->addSensorReading(reference, stringifiedValues, getSensorDelimiter(reference),
                                        rtc != 0 ? rtc : Wolk::currentRtc());
    });
}

INSTANTIATE_ADD_SENSOR_READING_FOR(char);
INSTANTIATE_ADD_SENSOR_READING_FOR(char*);
INSTANTIATE_ADD_SENSOR_READING_FOR(const char*);
INSTANTIATE_ADD_SENSOR_READING_FOR(bool);
INSTANTIATE_ADD_SENSOR_READING_FOR(float);
INSTANTIATE_ADD_SENSOR_READING_FOR(double);
INSTANTIATE_ADD_SENSOR_READING_FOR(signed int);
INSTANTIATE_ADD_SENSOR_READING_FOR(signed long int);
INSTANTIATE_ADD_SENSOR_READING_FOR(signed long long int);
INSTANTIATE_ADD_SENSOR_READING_FOR(unsigned int);
INSTANTIATE_ADD_SENSOR_READING_FOR(unsigned long int);
INSTANTIATE_ADD_SENSOR_READING_FOR(unsigned long long int);

void Wolk::addAlarm(const std::string& reference, const std::string& value, unsigned long long rtc)
{
    if (rtc == 0)
    {
        rtc = Wolk::currentRtc();
    }

    addToCommandBuffer([=]() -> void { m_dataService->addAlarm(reference, value, rtc); });
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

        m_dataService->addConfiguration(configuration, getConfigurationDelimiters());
        flushConfiguration();
    });
}

void Wolk::connect()
{
    addToCommandBuffer([=]() -> void {
        if (!m_connectivityService->connect())
        {
            return;
        }

        notifyConnected();

        publishFirmwareVersion();
        m_firmwareUpdateService->reportFirmwareUpdateResult();

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
{
    m_commandBuffer = std::unique_ptr<CommandBuffer>(new CommandBuffer());
}

void Wolk::addToCommandBuffer(std::function<void()> command)
{
    m_commandBuffer->pushCommand(std::make_shared<std::function<void()>>(command));
}

unsigned long long Wolk::currentRtc()
{
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
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

std::string Wolk::getSensorDelimiter(const std::string& reference)
{
    const auto sensors = m_device.getManifest().getSensors();
    auto sensorIt = std::find_if(sensors.cbegin(), sensors.cend(),
                                 [&](const SensorManifest& manifest) { return manifest.getReference() == reference; });

    if (sensorIt == sensors.end())
    {
        return "";
    }

    return sensorIt->getDelimiter();
}

std::map<std::string, std::string> Wolk::getConfigurationDelimiters()
{
    std::map<std::string, std::string> delimiters;

    const auto configurationItems = m_device.getManifest().getConfigurations();
    for (const auto& item : configurationItems)
    {
        if (!item.getDelimiter().empty())
        {
            delimiters[item.getReference()] = item.getDelimiter();
        }
    }

    return delimiters;
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

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
#include "connectivity/json/JsonSingleOutboundMessageFactory.h"
#include "model/ActuatorCommand.h"
#include "model/ActuatorStatus.h"
#include "model/Alarm.h"
#include "model/Device.h"
#include "model/SensorReading.h"
#include "service/FirmwareUpdateService.h"
#include "service/KeepAliveService.h"

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
    addSensorReading(reference, std::to_string(value), rtc);
}

template <> void Wolk::addSensorReading(const std::string& reference, std::string value, unsigned long long rtc)
{
    auto sensorReading = std::make_shared<SensorReading>(value, reference, rtc != 0 ? rtc : Wolk::currentRtc());
    addToCommandBuffer(
      [=]() -> void { m_persistence->putSensorReading(sensorReading->getReference(), sensorReading); });
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
                   [&](const T& value) -> std::string { return std::to_string(value); });

    auto sensorReading =
      std::make_shared<SensorReading>(stringifiedValues, reference, rtc != 0 ? rtc : Wolk::currentRtc());
    addToCommandBuffer(
      [=]() -> void { m_persistence->putSensorReading(sensorReading->getReference(), sensorReading); });
}

template <> void Wolk::addSensorReading(const std::string& reference, bool value, unsigned long long rtc)
{
    addSensorReading(reference, std::string(value ? "true" : "false"), rtc);
}

template <> void Wolk::addSensorReading(const std::string& reference, char* value, unsigned long long rtc)
{
    addSensorReading(reference, std::string(value), rtc);
}

template <> void Wolk::addSensorReading(const std::string& reference, const char* value, unsigned long long rtc)
{
    addSensorReading(reference, std::string(value), rtc);
}

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

    auto alarm = std::make_shared<Alarm>(value, reference, rtc);

    addToCommandBuffer([=]() -> void { m_persistence->putAlarm(alarm->getReference(), alarm); });
}

void Wolk::publishActuatorStatus(const std::string& reference)
{
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

    auto actuatorStatusWithRef =
      std::make_shared<ActuatorStatus>(actuatorStatus.getValue(), reference, actuatorStatus.getState());
    addToCommandBuffer([=]() -> void {
        addActuatorStatus(actuatorStatusWithRef);
        publishActuatorStatuses();
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
        publishActuatorStatuses();
        publishAlarms();
        publishSensorReadings();

        if (!m_persistence->isEmpty())
        {
            publish();
        }
    });
}

Wolk::Wolk(std::shared_ptr<ConnectivityService> connectivityService, std::shared_ptr<Persistence> persistence,
           std::shared_ptr<InboundMessageHandler> inboundMessageHandler,
           std::shared_ptr<OutboundServiceDataHandler> outboundServiceDataHandler, Device device)
: m_connectivityService(std::move(connectivityService))
, m_persistence(persistence)
, m_inboundMessageHandler(std::move(inboundMessageHandler))
, m_outboundServiceDataHandler(std::move(outboundServiceDataHandler))
, m_device(device)
, m_actuationHandlerLambda(nullptr)
, m_actuatorStatusProviderLambda(nullptr)
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

void Wolk::publishActuatorStatuses()
{
    for (const std::string& key : m_persistence->getGetActuatorStatusesKeys())
    {
        const auto actuatorStatus = m_persistence->getActuatorStatus(key);
        const std::shared_ptr<OutboundMessage> outboundMessage = m_outboundMessageFactory->make({actuatorStatus});

        if (outboundMessage && m_connectivityService->publish(outboundMessage))
        {
            m_persistence->removeActuatorStatus(actuatorStatus->getReference());
        }
    }
}

void Wolk::publishAlarms()
{
    for (const std::string& key : m_persistence->getAlarmsKeys())
    {
        const auto alarms = m_persistence->getAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);
        const std::shared_ptr<OutboundMessage> outboundMessage = m_outboundMessageFactory->make(alarms);

        if (outboundMessage && m_connectivityService->publish(outboundMessage))
        {
            m_persistence->removeAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);
        }
    }
}

void Wolk::publishSensorReadings()
{
    for (const std::string& key : m_persistence->getSensorReadingsKeys())
    {
        const auto sensorReadings = m_persistence->getSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);
        const std::shared_ptr<OutboundMessage> outboundMessage = m_outboundMessageFactory->make(sensorReadings);

        if (outboundMessage && m_connectivityService->publish(outboundMessage))
        {
            m_persistence->removeSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);
        }
    }
}

void Wolk::addActuatorStatus(std::shared_ptr<ActuatorStatus> actuatorStatus)
{
    m_persistence->putActuatorStatus(actuatorStatus->getReference(), actuatorStatus);
}

void Wolk::handleActuatorCommand(const ActuatorCommand& actuatorCommand)
{
    if (actuatorCommand.getType() == ActuatorCommand::Type::STATUS)
    {
        publishActuatorStatus(actuatorCommand.getReference());
    }
    else if (actuatorCommand.getType() == ActuatorCommand::Type::SET)
    {
        handleSetActuator(actuatorCommand);
        publishActuatorStatus(actuatorCommand.getReference());
    }
}

void Wolk::handleSetActuator(const ActuatorCommand& actuatorCommand)
{
    if (auto provider = m_actuationHandler.lock())
    {
        provider->handleActuation(actuatorCommand.getReference(), actuatorCommand.getValue());
    }
    else if (m_actuationHandlerLambda)
    {
        m_actuationHandlerLambda(actuatorCommand.getReference(), actuatorCommand.getValue());
    }
}

void Wolk::publishFirmwareVersion()
{
    if (m_firmwareUpdateService)
    {
        const auto firmwareVerion = m_firmwareUpdateService->getFirmwareVersion();
        const std::shared_ptr<OutboundMessage> outboundMessage =
          m_outboundMessageFactory->makeFromFirmwareVersion(firmwareVerion);

        if (!(outboundMessage && m_connectivityService->publish(outboundMessage)))
        {
            // TODO Log error
        }
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
}    // namespace wolkabout

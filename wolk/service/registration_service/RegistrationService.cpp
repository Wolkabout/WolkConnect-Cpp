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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied .
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wolk/service/registration_service/RegistrationService.h"

#include "core/utilities/Logger.h"

namespace wolkabout
{
RegistrationService::RegistrationService(RegistrationProtocol& protocol, ConnectivityService& connectivityService,
                                         ErrorService& errorService)
: m_protocol(protocol), m_connectivityService(connectivityService), m_errorService(errorService)
{
}

std::unique_ptr<ErrorMessage> RegistrationService::registerDevice(const std::string& deviceKey,
                                                                  DeviceIdentificationInformation information,
                                                                  Feeds feeds, Parameters parameters,
                                                                  Attributes attributes)
{
    LOG(TRACE) << METHOD_INFO;

    return nullptr;
}

std::unique_ptr<ErrorMessage> RegistrationService::removeDevices(const std::string& deviceKey,
                                                                 std::vector<std::string> deviceKeys)
{
    LOG(TRACE) << METHOD_INFO;

    return nullptr;
}

std::unique_ptr<std::vector<DeviceIdentificationInformation>> RegistrationService::obtainDevices(
  const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType, std::string externalId,
  std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain devices";

    // Create the message and the local query object
    const auto request = RegisteredDevicesRequestMessage{
      std::chrono::duration_cast<std::chrono::milliseconds>(timestampFrom.time_since_epoch()), deviceType, externalId};
    const auto query = DeviceQueryData{timestampFrom, deviceType, externalId};

    // Parse the message
    auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, request)};
    if (message == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Failed to generate outgoing `RegisteredDevicesRequest` message.";
        return nullptr;
    }

    // Send out the message
    if (!m_connectivityService.publish(message))
    {
        LOG(ERROR) << errorPrefix << " -> Failed to send the outgoing `RegisteredDevicesRequest` message.";
        return nullptr;
    }

    // Now that we have successfully published the message, put our object for receiving the response in the map, and
    // wait for it.
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        m_responses.emplace(query, nullptr);
    }
    auto lock = std::unique_lock<std::mutex>{m_mutex};
    m_conditionVariable.wait_for(lock, timeout);

    return {};
}

void RegistrationService::messageReceived(std::shared_ptr<Message> message) {}

const Protocol& RegistrationService::getProtocol()
{
    return m_protocol;
}
}    // namespace wolkabout

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
: m_protocol(protocol), m_connectivityService(connectivityService), m_errorService(errorService), m_running(false)
{
}

RegistrationService::~RegistrationService()
{
    stop();
}

void RegistrationService::start()
{
    m_running = true;
}

void RegistrationService::stop()
{
    m_running = false;
    m_conditionVariable.notify_all();
}

std::unique_ptr<ErrorMessage> RegistrationService::registerDevices(const std::string& deviceKey,
                                                                   const std::vector<DeviceRegistrationData>& devices,
                                                                   std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to register devices";

    // Check if the service is toggled on
    if (!m_running)
    {
        const auto errorMessage = "The service is not running.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Check that there's devices in the vector
    if (devices.empty())
    {
        const auto errorMessage = "The list of devices is empty.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Make the message that will be sent out
    const auto request = DeviceRegistrationMessage{devices};
    const auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, request)};
    if (message == nullptr)
    {
        const auto errorMessage = "Failed to generate the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Peek the number of errors
    auto errors = m_errorService.peekMessagesForDevice(deviceKey);

    // Send the message out
    if (!m_connectivityService.publish(message))
    {
        const auto errorMessage = "Failed to send the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Now wait for a response message
    if (m_errorService.awaitMessage(deviceKey, timeout))
        return errors > 1 ? m_errorService.obtainLastMessageForDevice(deviceKey) :
                            m_errorService.obtainFirstMessageForDevice(deviceKey);
    return nullptr;
}

std::unique_ptr<ErrorMessage> RegistrationService::removeDevices(const std::string& deviceKey,
                                                                 std::vector<std::string> deviceKeys,
                                                                 std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to remove a device";

    // Check if the service is toggled on
    if (!m_running)
    {
        const auto errorMessage = "The service is not running.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Make the message that will be sent out
    const auto request = DeviceRemovalMessage{deviceKeys};
    const auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, request)};
    if (message == nullptr)
    {
        const auto errorMessage = "Failed to generate the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Peek the number of errors
    auto errors = m_errorService.peekMessagesForDevice(deviceKey);

    // Send the message out
    if (!m_connectivityService.publish(message))
    {
        const auto errorMessage = "Failed to send the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return std::unique_ptr<ErrorMessage>{
          new ErrorMessage{deviceKey, errorMessage, std::chrono::system_clock::now()}};
    }

    // Now wait for a response message
    if (m_errorService.awaitMessage(deviceKey, timeout))
        return errors > 1 ? m_errorService.obtainLastMessageForDevice(deviceKey) :
                            m_errorService.obtainFirstMessageForDevice(deviceKey);
    return nullptr;
}

std::unique_ptr<std::vector<RegisteredDeviceInformation>> RegistrationService::obtainDevices(
  const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType, std::string externalId,
  std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain devices";

    // Check if the service is toggled on
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> The service is not running.";
        return nullptr;
    }

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

    // Lock the mutex for the map, just so the response arrival can't be faster than us putting the query data into the
    // map.
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        // Send out the message
        if (!m_connectivityService.publish(message))
        {
            LOG(ERROR) << errorPrefix << " -> Failed to send the outgoing `RegisteredDevicesRequest` message.";
            return nullptr;
        }
        m_responses.emplace(query, nullptr);
    }

    // Wait for the condition variable to be invoked
    {
        auto uniqueLock = std::unique_lock<std::mutex>{m_mutex};
        m_conditionVariable.wait_for(uniqueLock, timeout);
    }
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> Aborted execution because the service is stopping...";
        return nullptr;
    }

    // Check if there's a response in the map for us
    auto response = std::unique_ptr<RegisteredDevicesResponseMessage>{};
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        // Check if there's a value in the map
        const auto it = m_responses.find(query);
        if (it != m_responses.cend())
        {
            response = std::move(it->second);
            m_responses.erase(it);
        }
    }
    if (response == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Received no response message.";
        return nullptr;
    }

    // If there's some data that has been returned, return it.
    auto data = std::unique_ptr<std::vector<RegisteredDeviceInformation>>{new std::vector<RegisteredDeviceInformation>};
    for (const auto& device : response->getMatchingDevices())
        data->emplace_back(device);
    return data;
}

bool RegistrationService::obtainDevicesAsync(
  const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType, std::string externalId,
  std::function<void(const std::vector<RegisteredDeviceInformation>&)> callback)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain devices";

    // Check if the service is toggled on
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> The service is not running.";
        return false;
    }

    // Check whether the callback is actually set.
    if (!callback)
    {
        LOG(ERROR) << errorPrefix << " -> The user did not set the callback.";
        return false;
    }

    // Create the message and the local query object
    const auto request = RegisteredDevicesRequestMessage{
      std::chrono::duration_cast<std::chrono::milliseconds>(timestampFrom.time_since_epoch()), deviceType, externalId};
    const auto query = DeviceQueryData{timestampFrom, deviceType, externalId, callback};

    // Parse the message
    auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, request)};
    if (message == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Failed to generate outgoing `RegisteredDevicesRequest` message.";
        return false;
    }

    // Lock the mutex for the map, just so the response arrival can't be faster than us putting the query data into the
    // map.
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        // Send out the message
        if (!m_connectivityService.publish(message))
        {
            LOG(ERROR) << errorPrefix << " -> Failed to send the outgoing `RegisteredDevicesRequest` message.";
            return false;
        }
        m_responses.emplace(query, nullptr);
        return true;
    }
}

void RegistrationService::messageReceived(std::shared_ptr<Message> message)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to process received message";

    // Try to parse the incoming message
    auto parsedMessage = m_protocol.parseRegisteredDevicesResponse(message);
    if (parsedMessage == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> The message could not be parsed.";
        return;
    }

    // Make the query object from the message
    auto query = DeviceQueryData{TimePoint(parsedMessage->getTimestampFrom()), parsedMessage->getDeviceType(),
                                 parsedMessage->getExternalId()};
    // And now look whether there's such a query object in the map
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        const auto it = m_responses.find(query);
        if (it == m_responses.cend())
        {
            LOG(ERROR) << errorPrefix << " -> There is no record of request being sent out for this response.";
            return;
        }

        // Check whether there exists a callback.
        if (it->first.getCallback())
            // Just invoke the callback
            it->first.getCallback()(parsedMessage->getMatchingDevices());
        else
            // Place the value in the map
            m_responses[query] = std::move(parsedMessage);
    }
    m_conditionVariable.notify_one();
}

const Protocol& RegistrationService::getProtocol()
{
    return m_protocol;
}
}    // namespace wolkabout

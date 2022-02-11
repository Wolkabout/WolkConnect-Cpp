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

#include <algorithm>

namespace wolkabout
{
namespace connect
{
RegistrationService::RegistrationService(RegistrationProtocol& protocol, ConnectivityService& connectivityService)
: m_protocol(protocol), m_connectivityService(connectivityService), m_running(false)
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
    m_registeredDevicesCV.notify_all();
    m_childrenSyncDevicesCV.notify_all();
}

bool RegistrationService::registerDevices(
  const std::string& deviceKey, const std::vector<DeviceRegistrationData>& devices,
  std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)> callback)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to register devices";

    // Check if the service is toggled on
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> The service is not running.";
        return false;
    }

    // Check that there's devices in the vector and that their names are not empty
    if (devices.empty())
    {
        LOG(ERROR) << errorPrefix << " -> The list of devices is empty.";
        return false;
    }
    auto deviceNames = std::vector<std::string>{};
    for (const auto& device : devices)
    {
        if (device.key.empty())
        {
            LOG(ERROR) << errorPrefix << " -> One of the devices has an empty name.";
            return false;
        }
        deviceNames.emplace_back(device.key);
    }
    std::sort_heap(deviceNames.begin(), deviceNames.end());

    // Make the message that will be sent out
    const auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, DeviceRegistrationMessage{devices})};
    if (message == nullptr)
    {
        const auto errorMessage = "Failed to generate the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return false;
    }

    // Send the message out
    if (!m_connectivityService.publish(message))
    {
        const auto errorMessage = "Failed to send the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return false;
    }

    // Emplace the callback in the map
    {
        std::lock_guard<std::mutex> lock{m_deviceRegistrationMutex};
        m_deviceRegistrationCallbacks.emplace(deviceNames, std::move(callback));
    }
    return true;
}

bool RegistrationService::removeDevices(const std::string& deviceKey, std::vector<std::string> deviceKeys)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to remove a device";

    // Check if the service is toggled on
    if (!m_running)
    {
        const auto errorMessage = "The service is not running.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return false;
    }

    // Make the message that will be sent out
    const auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, DeviceRemovalMessage{deviceKeys})};
    if (message == nullptr)
    {
        const auto errorMessage = "Failed to generate the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return false;
    }

    // Send the message out
    if (!m_connectivityService.publish(message))
    {
        const auto errorMessage = "Failed to send the outgoing message.";
        LOG(ERROR) << errorPrefix << " -> " << errorMessage;
        return false;
    }
    return true;
}

std::shared_ptr<std::vector<std::string>> RegistrationService::obtainChildren(const std::string& deviceKey,
                                                                              std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain children";

    // Check if the service is toggled on
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> The service is not running.";
        return nullptr;
    }

    // Parse the message
    auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, ChildrenSynchronizationRequestMessage{})};
    if (message == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Failed to generate outgoing `ChildrenSynchronizationRequestMessage`.";
        return nullptr;
    }

    // Lock the mutex for the map, just so the response arrival can't be faster than us putting the query data into the
    // map.
    std::atomic_bool called{false};
    auto list = std::make_shared<std::vector<std::string>>();
    {
        std::lock_guard<std::mutex> lock{m_childrenSyncDevicesMutex};
        // Send out the message
        if (!m_connectivityService.publish(message))
        {
            LOG(ERROR) << errorPrefix << " -> Failed to send the outgoing `ChildrenSynchronizationRequestMessage`.";
            return nullptr;
        }
        if (m_queries.find(deviceKey) == m_queries.cend())
            m_queries.emplace(deviceKey, std::queue<std::function<void(std::vector<std::string>)>>{});
        auto weakList = std::weak_ptr<std::vector<std::string>>{list};
        m_queries[deviceKey].push([this, &called, weakList](const std::vector<std::string>& children) {
            if (auto childrenList = weakList.lock())
            {
                for (const auto& child : children)
                    childrenList->emplace_back(child);
                called = true;
                m_childrenSyncDevicesCV.notify_one();
            }
        });
    }

    // Wait for the condition variable to be invoked
    {
        auto uniqueLock = std::unique_lock<std::mutex>{m_registeredDevicesMutex};
        m_childrenSyncDevicesCV.wait_for(uniqueLock, timeout);
    }
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> Aborted execution because the service is stopping...";
        return nullptr;
    }
    if (!called)
        return nullptr;
    else
        return list;
}

bool RegistrationService::obtainChildrenAsync(const std::string& deviceKey,
                                              std::function<void(std::vector<std::string>)> callback)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain children";

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

    // Parse the message
    auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, ChildrenSynchronizationRequestMessage{})};
    if (message == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Failed to generate outgoing `ChildrenSynchronizationRequestMessage`.";
        return false;
    }

    // Lock the mutex for the map, just so the response arrival can't be faster than us putting the query data into the
    // map.
    {
        std::lock_guard<std::mutex> lock{m_childrenSyncDevicesMutex};
        // Send out the message
        if (!m_connectivityService.publish(message))
        {
            LOG(ERROR) << errorPrefix << " -> Failed to send the outgoing `ChildrenSynchronizationRequestMessage`.";
            return false;
        }
        if (m_queries.find(deviceKey) == m_queries.cend())
            m_queries.emplace(deviceKey, std::queue<std::function<void(std::vector<std::string>)>>{});
        m_queries[deviceKey].push(callback);
        return true;
    }
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

    // Parse the message
    const auto query = DeviceQueryData{timestampFrom, deviceType, externalId};
    auto request = RegisteredDevicesRequestMessage{
      std::chrono::duration_cast<std::chrono::milliseconds>(timestampFrom.time_since_epoch()), deviceType, externalId};
    auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, std::move(request))};
    if (message == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Failed to generate outgoing `RegisteredDevicesRequest` message.";
        return nullptr;
    }

    // Lock the mutex for the map, just so the response arrival can't be faster than us putting the query data into the
    // map.
    {
        std::lock_guard<std::mutex> lock{m_registeredDevicesMutex};
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
        auto uniqueLock = std::unique_lock<std::mutex>{m_registeredDevicesMutex};
        m_registeredDevicesCV.wait_for(uniqueLock, timeout);
    }
    if (!m_running)
    {
        LOG(ERROR) << errorPrefix << " -> Aborted execution because the service is stopping...";
        return nullptr;
    }

    // Check if there's a response in the map for us
    auto response = std::unique_ptr<RegisteredDevicesResponseMessage>{};
    {
        std::lock_guard<std::mutex> lock{m_registeredDevicesMutex};
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
        std::lock_guard<std::mutex> lock{m_registeredDevicesMutex};
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

    const auto messageType = m_protocol.getMessageType(*message);
    switch (messageType)
    {
    case MessageType::DEVICE_REGISTRATION_RESPONSE:
    {
        auto parsedMessage = m_protocol.parseDeviceRegistrationResponse(message);
        if (parsedMessage == nullptr)
            LOG(ERROR) << errorPrefix << " -> The message could not be parsed.";
        else
            handleDeviceRegistrationResponse(std::move(parsedMessage));
        break;
    }
    case MessageType::REGISTERED_DEVICES_RESPONSE:
    {
        auto parsedMessage = m_protocol.parseRegisteredDevicesResponse(message);
        if (parsedMessage == nullptr)
            LOG(ERROR) << errorPrefix << " -> The message could not be parsed.";
        else
            handleRegisteredDevicesResponse(std::move(parsedMessage));
        break;
    }
    default:
    {
        LOG(ERROR) << errorPrefix << " -> Received message of type this handler can not handle.";
    }
    }
}

const Protocol& RegistrationService::getProtocol()
{
    return m_protocol;
}

void RegistrationService::handleDeviceRegistrationResponse(
  std::unique_ptr<DeviceRegistrationResponseMessage> responseMessage)
{
    // Check nullptr
    if (responseMessage == nullptr)
        return;

    // Take out the device names and find a callback
    auto deviceNames = std::vector<std::string>{};
    std::copy(responseMessage->getSuccess().cbegin(), responseMessage->getSuccess().cend(),
              std::back_inserter(deviceNames));
    std::copy(responseMessage->getFailed().cbegin(), responseMessage->getFailed().cend(),
              std::back_inserter(deviceNames));
    std::sort(deviceNames.begin(), deviceNames.end());

    // Look for callbacks
    {
        std::lock_guard<std::mutex> lock{m_deviceRegistrationMutex};
        const auto it = m_deviceRegistrationCallbacks.find(deviceNames);
        if (it == m_deviceRegistrationCallbacks.cend())
            return;
        const auto callback = std::move(it->second);
        const auto success = responseMessage->getSuccess();
        const auto failed = responseMessage->getFailed();
        m_commandBuffer.pushCommand(
          std::make_shared<std::function<void()>>([callback, success, failed] { callback(success, failed); }));
        m_deviceRegistrationCallbacks.erase(it);
    }
}

void RegistrationService::handleRegisteredDevicesResponse(
  std::unique_ptr<RegisteredDevicesResponseMessage> responseMessage)
{
    // Check nullptr
    if (responseMessage == nullptr)
        return;

    // Make the query object from the message
    auto query = DeviceQueryData{TimePoint(responseMessage->getTimestampFrom()), responseMessage->getDeviceType(),
                                 responseMessage->getExternalId()};
    // And now look whether there's such a query object in the map
    {
        std::lock_guard<std::mutex> lock{m_registeredDevicesMutex};
        const auto it = m_responses.find(query);
        if (it == m_responses.cend())
            return;

        // Check whether there exists a callback.
        if (it->first.getCallback())
        {    // Just invoke the callback
            const auto callback = std::move(it->first.getCallback());
            const auto devices = responseMessage->getMatchingDevices();
            m_commandBuffer.pushCommand(
              std::make_shared<std::function<void()>>([callback, devices]() { callback(devices); }));
        }
        else
        {
            // Place the value in the map
            m_responses[query] = std::move(responseMessage);
        }
    }
    m_registeredDevicesCV.notify_one();
}
}    // namespace connect
}    // namespace wolkabout

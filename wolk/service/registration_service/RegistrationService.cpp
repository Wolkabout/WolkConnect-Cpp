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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied .
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wolk/service/registration_service/RegistrationService.h"

#include "core/utility/Logger.h"

#include <algorithm>

using namespace wolkabout::legacy;

namespace wolkabout
{
namespace connect
{
DeviceQueryData::DeviceQueryData(TimePoint timestampFrom, std::string deviceType, std::string externalId,
                                 std::function<void(const std::vector<RegisteredDeviceInformation>&)> callback)
: m_timestampFrom(std::move(std::chrono::duration_cast<std::chrono::milliseconds>(timestampFrom.time_since_epoch())))
, m_deviceType(std::move(deviceType))
, m_externalId(std::move(externalId))
, m_callback(std::move(callback))
{
}

const TimePoint& DeviceQueryData::getTimestampFrom() const
{
    return m_timestampFrom;
}

const std::string& DeviceQueryData::getDeviceType() const
{
    return m_deviceType;
}

const std::string& DeviceQueryData::getExternalId() const
{
    return m_externalId;
}

const std::function<void(const std::vector<RegisteredDeviceInformation>&)>& DeviceQueryData::getCallback() const
{
    return m_callback;
}

bool DeviceQueryData::operator==(const DeviceQueryData& rvalue) const
{
    return m_timestampFrom == rvalue.m_timestampFrom && m_deviceType == rvalue.m_deviceType &&
           m_externalId == rvalue.m_externalId;
}

std::size_t DeviceQueryDataHash::operator()(const DeviceQueryData& data) const
{
    auto timestamp =
      std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(data.getTimestampFrom().time_since_epoch().count()));
    auto deviceType = std::hash<std::string>{}(data.getDeviceType());
    auto externalId = std::hash<std::string>{}(data.getExternalId());
    return timestamp ^ (deviceType << 1) ^ (externalId << 2);
}

RegistrationService::RegistrationService(RegistrationProtocol& protocol, ConnectivityService& connectivityService)
: m_exitCondition{false}, m_protocol(protocol), m_connectivityService(connectivityService)
{
}

RegistrationService::~RegistrationService()
{
    stop();
}

void RegistrationService::start() {}

void RegistrationService::stop()
{
    m_exitCondition = true;
    m_childrenSyncDevicesCV.notify_all();
    m_registeredDevicesCV.notify_all();
}

bool RegistrationService::registerDevices(
  const std::string& deviceKey, const std::vector<DeviceRegistrationData>& devices,
  std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)> callback)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to register devices";

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
    std::sort(deviceNames.begin(), deviceNames.end());

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
    auto callbackIt = std::vector<std::function<void(std::vector<std::string>)>>::iterator{};
    auto called = std::make_shared<std::atomic_bool>(false);
    auto list = std::make_shared<std::vector<std::string>>();
    {
        std::lock_guard<std::mutex> lock{m_childrenSyncDevicesMutex};
        // Send out the message
        if (!m_connectivityService.publish(message))
        {
            LOG(ERROR) << errorPrefix << " -> Failed to send the outgoing `ChildrenSynchronizationRequestMessage`.";
            return nullptr;
        }
        auto weakCalled = std::weak_ptr<std::atomic_bool>{called};
        auto weakList = std::weak_ptr<std::vector<std::string>>{list};
        m_childrenSynchronizationCallbacks[deviceKey].emplace_back(
          [this, weakCalled, weakList](const std::vector<std::string>& children) {
              // Check the flag
              auto calledFlag = weakCalled.lock();
              if (calledFlag == nullptr)
                  return;

              // Check the list
              auto childrenList = weakList.lock();
              if (childrenList == nullptr)
                  return;

              // Emplace the values in the list and change the flag
              for (const auto& child : children)
                  childrenList->emplace_back(child);
              *calledFlag = true;
              m_childrenSyncDevicesCV.notify_one();
          });
        callbackIt = std::prev(m_childrenSynchronizationCallbacks[deviceKey].end());
    }

    // Wait for the condition variable to be invoked
    {
        auto uniqueLock = std::unique_lock<std::mutex>{m_registeredDevicesMutex};
        m_childrenSyncDevicesCV.wait_for(uniqueLock, timeout, [&] { return *called || m_exitCondition; });
    }
    if (!*called)
    {
        // Remove the callback as it wasn't called
        std::lock_guard<std::mutex> lockGuard{m_registeredDevicesMutex};
        m_childrenSynchronizationCallbacks[deviceKey].erase(callbackIt);
        return nullptr;
    }
    else
    {
        return list;
    }
}

bool RegistrationService::obtainChildrenAsync(const std::string& deviceKey,
                                              std::function<void(std::vector<std::string>)> callback)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain children";

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
        m_childrenSynchronizationCallbacks[deviceKey].emplace_back(callback);
        return true;
    }
}

std::unique_ptr<std::vector<RegisteredDeviceInformation>> RegistrationService::obtainDevices(
  const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType, std::string externalId,
  std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to obtain devices";

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
        m_deviceRegistrationResponses.emplace(query, nullptr);
    }

    // Wait for the condition variable to be invoked
    {
        auto uniqueLock = std::unique_lock<std::mutex>{m_registeredDevicesMutex};
        m_registeredDevicesCV.wait_for(uniqueLock, timeout);
    }

    // Check if there's a response in the map for us
    auto response = std::unique_ptr<RegisteredDevicesResponseMessage>{};
    {
        std::lock_guard<std::mutex> lock{m_registeredDevicesMutex};
        // Check if there's a value in the map
        const auto it = m_deviceRegistrationResponses.find(query);
        if (it != m_deviceRegistrationResponses.cend())
        {
            response = std::move(it->second);
            m_deviceRegistrationResponses.erase(it);
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
        m_deviceRegistrationResponses.emplace(query, nullptr);
        return true;
    }
}

void RegistrationService::messageReceived(std::shared_ptr<Message> message)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to process received message";

    const auto messageType = m_protocol.getMessageType(*message);
    const auto deviceKey = m_protocol.getDeviceKey(*message);
    switch (messageType)
    {
    case MessageType::CHILDREN_SYNCHRONIZATION_RESPONSE:
    {
        auto parsedMessage = m_protocol.parseChildrenSynchronizationResponse(message);
        if (parsedMessage == nullptr)
            LOG(ERROR) << errorPrefix << " -> The message could not be parsed.";
        else
            handleChildrenSynchronizationResponse(deviceKey, std::move(parsedMessage));
        break;
    }
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

void RegistrationService::handleChildrenSynchronizationResponse(
  const std::string& deviceKey, std::unique_ptr<ChildrenSynchronizationResponseMessage> responseMessage)
{
    // Check nullptr
    if (responseMessage == nullptr)
        return;

    // Look for any callbacks
    std::lock_guard<std::mutex> lockGuard{m_childrenSyncDevicesMutex};
    if (!m_childrenSynchronizationCallbacks[deviceKey].empty())
    {
        // Take the first callback from the queue
        const auto callback = m_childrenSynchronizationCallbacks[deviceKey].front();
        m_childrenSynchronizationCallbacks[deviceKey].erase(m_childrenSynchronizationCallbacks[deviceKey].begin());

        // Make copy of the children devices
        const auto children = responseMessage->getChildren();

        // And invoke the callback
        m_commandBuffer.pushCommand(
          std::make_shared<std::function<void()>>([callback, children] { callback(children); }));
    }
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
        const auto it = m_deviceRegistrationResponses.find(query);
        if (it == m_deviceRegistrationResponses.cend())
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
            m_deviceRegistrationResponses[query] = std::move(responseMessage);
        }
    }
    m_registeredDevicesCV.notify_one();
}
}    // namespace connect
}    // namespace wolkabout

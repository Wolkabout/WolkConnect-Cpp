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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H
#define WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H

#include "core/MessageListener.h"
#include "core/Types.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/protocol/RegistrationProtocol.h"
#include "core/utilities/CommandBuffer.h"
#include "core/utilities/Service.h"
#include "wolk/service/error/ErrorService.h"

#include <unordered_map>

namespace wolkabout
{
namespace connect
{
// This struct is used to group the query information that is used to generate a request towards the platform.
// Based on these, the request will be linked with a response.
struct DeviceQueryData
{
public:
    explicit DeviceQueryData(TimePoint timestampFrom, std::string deviceType = "", std::string externalId = "",
                             std::function<void(const std::vector<RegisteredDeviceInformation>&)> callback = {});

    const TimePoint& getTimestampFrom() const;

    const std::string& getDeviceType() const;

    const std::string& getExternalId() const;

    const std::function<void(const std::vector<RegisteredDeviceInformation>&)>& getCallback() const;

    bool operator==(const DeviceQueryData& rvalue) const;

private:
    TimePoint m_timestampFrom;
    std::string m_deviceType;
    std::string m_externalId;

    // Optionally, the query can have a callback that should be called
    std::function<void(const std::vector<RegisteredDeviceInformation>&)> m_callback;
};

// Define a hash function for the DeviceQueryData.
struct DeviceQueryDataHash
{
public:
    std::size_t operator()(const DeviceQueryData& data) const;
};

// Define a hash function for a vector of strings.
struct StringVectorHash
{
public:
    std::size_t operator()(const std::vector<std::string>& data) const
    {
        auto hash = std::size_t{0};
        for (const auto& entry : data)
            hash ^= std::hash<std::string>()(entry);
        return hash;
    }
};

/**
 * This is the service that is responsible for registering/removing devices, and also obtaining information about
 * devices.
 */
class RegistrationService : public MessageListener, public Service
{
public:
    /**
     * Default parameter constructor.
     *
     * @param protocol The protocol this service will follow.
     * @param connectivityService The connectivity service used to send outgoing messages.
     */
    explicit RegistrationService(RegistrationProtocol& protocol, ConnectivityService& connectivityService);

    /**
     * Overridden constructor. Will stop all running condition variables.
     */
    ~RegistrationService() override;

    /**
     * This is the overridden method from the `utilities::Service` interface.
     * This method will allow the service to function properly.
     */
    void start() override;

    /**
     * This is the overridden method from the `utilities::Service` interface.
     * This method will stop the workings of the service, close all ongoing condition variables.
     */
    void stop() override;

    /**
     * This method is used to send a device registration request.
     *
     * @param deviceKey The key of the device trying to register the devices.
     * @param devices The list of devices that the user would like to register.
     * @param callback The callback which will be invoked with devices that are registered, and ones that are not.
     * @return Whether the `DeviceRegistrationMessage` has been sent out.
     */
    virtual bool registerDevices(
      const std::string& deviceKey, const std::vector<DeviceRegistrationData>& devices,
      std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)> callback);

    /**
     * This method is used to send a device deletion request.
     *
     * @param deviceKey The key of the device trying to delete the devices.
     * @param deviceKeys The list of device keys that should be deleted.
     * @return Whether the `DeviceRemovalMessage` has been sent out.
     */
    virtual bool removeDevices(const std::string& deviceKey, std::vector<std::string> deviceKeys);

    /**
     * This method is used to obtain the list of children of a device. This is the synchronous version of the method
     * that will attempt to await the response.
     *
     * @param deviceKey The key of the device trying to obtain the list of children.
     * @param timeout The maximum wait the method will await the response.
     * @return This list of children obtained. Will be a {@code: nullptr} if unable to obtain children, empty vector if
     * the platform returned no devices, or filled with devices if everything has gone successfully.
     */
    virtual std::shared_ptr<std::vector<std::string>> obtainChildren(const std::string& deviceKey,
                                                                     std::chrono::milliseconds timeout);

    /**
     * This method is used to obtain the list of children of a device. This is the asynchronous version of the method
     * that wil call a callback once a response has been received.
     *
     * @param deviceKey The key of the device trying to obtain the list of children.
     * @param callback The callback that will be invoked once a response has been received.
     * @return Whether the request was successfully sent out. If this is false, that means that the callback will never
     * be called.
     */
    virtual bool obtainChildrenAsync(const std::string& deviceKey,
                                     std::function<void(std::vector<std::string>)> callback);

    /**
     * This method is used to obtain a list of devices. This is the synchronous version of the method that will attempt
     * to await the response.
     *
     * @param deviceKey The key of the device trying to obtain the list of devices.
     * @param timestampFrom The timestamp from which devices will be queried.
     * @param deviceType The type of devices that are queried.
     * @param externalId The external id of a device, if you have such a value to query a single device.
     * @param timeout The maximum wait the method will await the response.
     * @return The list of devices obtained. Will be a {@code: nullptr} if unable to obtain devices, empty vector if the
     * platform returned no devices, or filled with devices if everything has gone successfully.
     */
    virtual std::unique_ptr<std::vector<RegisteredDeviceInformation>> obtainDevices(const std::string& deviceKey,
                                                                                    TimePoint timestampFrom,
                                                                                    std::string deviceType,
                                                                                    std::string externalId,
                                                                                    std::chrono::milliseconds timeout);

    /**
     * This method is used to obtain a list of devices. This is the asynchronous version of the method that will call a
     * callback once a response has been received.
     *
     * @param deviceKey The key of the device trying to obtain the list of devices.
     * @param timestampFrom The timestamp from which devices will be queried.
     * @param deviceType The type of devices that are queried.
     * @param externalId The external id of a device, if you have such a value to query a single device.
     * @param callback The callback that will be invoked once a response has been received.
     * @return Whether the request was successfully sent out. If this is false, that means that the callback will never
     * be called.
     */
    virtual bool obtainDevicesAsync(const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType,
                                    std::string externalId,
                                    std::function<void(const std::vector<RegisteredDeviceInformation>&)> callback);

    /**
     * This method is overridden from the `MessageListener` interface.
     * This is the method that is invoked once a message has arrived for this listener.
     *
     * @param message The message that has been routed to this listener.
     */
    void messageReceived(std::shared_ptr<Message> message) override;

    /**
     * This method is overridden from the `MessageListener` interface.
     * This is the getter for the protocol this listener is using.
     *
     * @return Reference to the protocol this listener is using.
     */
    const Protocol& getProtocol() override;

private:
    /**
     * This is the internal method that is invoked to handle the received `DeviceRegistrationResponseMessage`.
     *
     * @param responseMessage The received `DeviceRegistrationResponseMessage`.
     */
    void handleDeviceRegistrationResponse(std::unique_ptr<DeviceRegistrationResponseMessage> responseMessage);

    /**
     * This is the internal method that is invoked to handle the received `RegisteredDevicesResponseMessage`.
     *
     * @param responseMessage The received `RegisteredDevicesResponseMessage`.
     */
    void handleRegisteredDevicesResponse(std::unique_ptr<RegisteredDevicesResponseMessage> responseMessage);

    // Here we store the actual protocol.
    RegistrationProtocol& m_protocol;

    // We need to use the connectivity service to send out the messages.
    ConnectivityService& m_connectivityService;

    // Make place for the children requests
    std::mutex m_childrenSyncDevicesMutex;
    std::condition_variable m_childrenSyncDevicesCV;

    // Make place for the device registration responses
    std::mutex m_deviceRegistrationMutex;
    std::unordered_map<std::vector<std::string>,
                       std::function<void(std::vector<std::string>, std::vector<std::string>)>, StringVectorHash>
      m_deviceRegistrationCallbacks;

    // Make place for the requests for devices
    std::mutex m_registeredDevicesMutex;
    std::condition_variable m_registeredDevicesCV;
    std::unordered_map<DeviceQueryData, std::unique_ptr<RegisteredDevicesResponseMessage>, DeviceQueryDataHash>
      m_responses;

    // Have a command buffer for calling some callbacks
    CommandBuffer m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H

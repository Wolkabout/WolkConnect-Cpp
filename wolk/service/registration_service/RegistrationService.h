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

#include "core/Types.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/connectivity/InboundMessageHandler.h"
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/protocol/RegistrationProtocol.h"
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
                             std::function<void(const std::vector<RegisteredDeviceInformation>&)> callback = {})
    : m_timestampFrom(
        std::move(std::chrono::duration_cast<std::chrono::milliseconds>(timestampFrom.time_since_epoch())))
    , m_deviceType(std::move(deviceType))
    , m_externalId(std::move(externalId))
    , m_callback(std::move(callback))
    {
    }

    const TimePoint& getTimestampFrom() const { return m_timestampFrom; }

    const std::string& getDeviceType() const { return m_deviceType; }

    const std::string& getExternalId() const { return m_externalId; }

    const std::function<void(const std::vector<RegisteredDeviceInformation>&)>& getCallback() const
    {
        return m_callback;
    }

    bool operator==(const DeviceQueryData& rvalue) const
    {
        return m_timestampFrom == rvalue.m_timestampFrom && m_deviceType == rvalue.m_deviceType &&
               m_externalId == rvalue.m_externalId;
    }

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
    std::size_t operator()(const DeviceQueryData& data) const
    {
        auto timestamp =
          std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(data.getTimestampFrom().time_since_epoch().count()));
        auto deviceType = std::hash<std::string>{}(data.getDeviceType());
        auto externalId = std::hash<std::string>{}(data.getExternalId());
        return timestamp ^ (deviceType << 1) ^ (externalId << 2);
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
     * @param errorService The error service used to listen to error messages.
     */
    explicit RegistrationService(RegistrationProtocol& protocol, ConnectivityService& connectivityService,
                                 ErrorService& errorService);

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
     * @param timeout The time the method will maximally wait for an error to appear.
     * @return If nothing has gone wrong, this will be {@code: nullptr}. Otherwise a value will be passed, with a
     * further explanation of the error.
     */
    virtual std::unique_ptr<ErrorMessage> registerDevices(const std::string& deviceKey,
                                                          const std::vector<DeviceRegistrationData>& devices,
                                                          std::chrono::milliseconds timeout);

    /**
     * This method is used to send a device deletion request.
     *
     * @param deviceKey The key of the device trying to delete the devices.
     * @param deviceKeys The list of device keys that should be deleted.
     * @param timeout The time the method will maximally wait for an error to appear.
     * @return If nothing has gone wrong, this will be {@code: nullptr}. Otherwise a value will be passed, with a
     * further explanation of the error.
     */
    virtual std::unique_ptr<ErrorMessage> removeDevices(const std::string& deviceKey,
                                                        std::vector<std::string> deviceKeys,
                                                        std::chrono::milliseconds timeout);

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
    // Here we store the actual protocol.
    RegistrationProtocol& m_protocol;

    // We need to use the connectivity service to send out the messages.
    ConnectivityService& m_connectivityService;

    // And here we have the reference of the error service.
    ErrorService& m_errorService;

    // Make place for the running status
    std::atomic_bool m_running;

    // Make place for the requests for devices
    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    std::unordered_map<DeviceQueryData, std::unique_ptr<RegisteredDevicesResponseMessage>, DeviceQueryDataHash>
      m_responses;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H

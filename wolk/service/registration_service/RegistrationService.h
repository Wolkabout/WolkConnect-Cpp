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

#include "core/InboundMessageHandler.h"
#include "core/Types.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/protocol/RegistrationProtocol.h"
#include "wolk/service/error/ErrorService.h"

#include <unordered_map>

namespace wolkabout
{
// Making some type aliases that will make method signatures more readable
using Feeds = std::vector<Feed>;
using Parameters = std::map<ParameterName, std::string>;
using Attributes = std::vector<Attribute>;
// This struct is used to group the information that identifies a new device.
struct DeviceIdentificationInformation
{
public:
    explicit DeviceIdentificationInformation(std::string name, std::string key = "", std::string guid = "")
    : m_name(std::move(name)), m_key(std::move(key)), m_guid(std::move(guid))
    {
    }

    const std::string& getName() const { return m_name; }

    const std::string& getKey() const { return m_key; }

    const std::string& getGuid() const { return m_guid; }

private:
    std::string m_name;
    std::string m_key;
    std::string m_guid;
};
// This struct is used to group the query information that is used to generate a request towards the platform.
// Based on these, the request will be linked with a response.
struct DeviceQueryData
{
public:
    explicit DeviceQueryData(TimePoint timestampFrom, std::string deviceType = "", std::string externalId = "")
    : m_timestampFrom(std::move(timestampFrom))
    , m_deviceType(std::move(deviceType))
    , m_externalId(std::move(externalId))
    {
    }

    const TimePoint& getTimestampFrom() const { return m_timestampFrom; }

    const std::string& getDeviceType() const { return m_deviceType; }

    const std::string& getExternalId() const { return m_externalId; }

    bool operator==(const DeviceQueryData& rvalue) const
    {
        return m_timestampFrom == rvalue.m_timestampFrom && m_deviceType == rvalue.m_deviceType &&
               m_externalId == rvalue.m_externalId;
    }

private:
    TimePoint m_timestampFrom;
    std::string m_deviceType;
    std::string m_externalId;
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
class RegistrationService : public MessageListener
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
     * This method is used to send a device registration request.
     *
     * @param deviceKey The key of the device trying to register the device.
     * @param information The basic identifying information about a device.
     * @param feeds The list of feeds that the device has.
     * @param parameters The list of parameters values for the device.
     * @param attributes The list of attributes that the device has.
     * @return If nothing has gone wrong, this will be {@code: nullptr}. Otherwise a value will be passed, with a
     * further explanation of the error.
     */
    std::unique_ptr<ErrorMessage> registerDevice(const std::string& deviceKey,
                                                 DeviceIdentificationInformation information, Feeds feeds,
                                                 Parameters parameters, Attributes attributes);

    /**
     * This method is used to send a device deletion request.
     *
     * @param deviceKey The key of the device trying to delete the devices.
     * @param deviceKeys The list of device keys that should be deleted.
     * @return If nothing has gone wrong, this will be {@code: nullptr}. Otherwise a value will be passed, with a
     * further explanation of the error.
     */
    std::unique_ptr<ErrorMessage> removeDevices(const std::string& deviceKey, std::vector<std::string> deviceKeys);

    /**
     * This method is used to obtain a list of devices.
     *
     * @param deviceKey The key of the device trying to obtain the list of devices.
     * @param timestampFrom The timestamp from which devices will be queried.
     * @param deviceType The type of devices that are queried.
     * @param externalId The external id of a device, if you have such a value to query a single device.
     * @return The list of devices obtained. Will be a {@code: nullptr} if unable to obtain devices, empty vector if the
     * platform returned no devices, or filled with devices if everything has gone successfully.
     */
    std::unique_ptr<std::vector<DeviceIdentificationInformation>> obtainDevices(
      const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType = {}, std::string externalId = {},
      std::chrono::milliseconds timeout = std::chrono::milliseconds{100});

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

    // Make place for the requests for devices
    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    std::unordered_map<DeviceQueryData, std::unique_ptr<RegisteredDevicesResponseMessage>, DeviceQueryDataHash>
      m_responses;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H

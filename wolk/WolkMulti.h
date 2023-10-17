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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WOLK_MULTI_H
#define WOLK_MULTI_H

#include "core/connectivity/InboundPlatformMessageHandler.h"
#include "core/utility/StringUtils.h"
#include "wolk/WolkBuilder.h"
#include "wolk/WolkInterface.h"

#include <algorithm>

namespace wolkabout
{
class Device;

namespace connect
{
class WolkBuilder;

class WolkMulti : public WolkInterface
{
    friend class WolkBuilder;

public:
    static WolkBuilder newBuilder(std::vector<Device> devices = {});

    bool addDevice(const Device& device);

    template <typename T>
    void addReading(const std::string& deviceKey, const std::string& reference, T value, std::uint64_t rtc = 0);

    void addReading(const std::string& deviceKey, const std::string& reference, std::string value,
                    std::uint64_t rtc = 0);

    template <typename T>
    void addReading(const std::string& deviceKey, const std::string& reference, const std::vector<T>& values,
                    std::uint64_t rtc = 0);

    void addReading(const std::string& deviceKey, const std::string& reference, const std::vector<std::string>& values,
                    std::uint64_t rtc = 0);

    void addReading(const std::string& deviceKey, const Reading& reading);

    void addReadings(const std::string& deviceKey, const std::vector<Reading>& readings);

    void pullFeedValues(const std::string& deviceKey);
    void pullParameters(const std::string& deviceKey);

    void registerFeed(const std::string& deviceKey, const Feed& feed);
    void registerFeeds(const std::string& deviceKey, const std::vector<Feed>& feeds);

    void removeFeed(const std::string& deviceKey, const std::string& reference);
    void removeFeeds(const std::string& deviceKey, const std::vector<std::string>& references);

    void addAttribute(const std::string& deviceKey, Attribute attribute);

    void updateParameter(const std::string& deviceKey, Parameter parameters);

    bool registerDevice(const DeviceRegistrationData& device,
                        std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)> callback);

    bool registerDevices(
      const std::vector<DeviceRegistrationData>& devices,
      std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)> callback);

    bool removeDevice(const std::string& deviceKey, const std::string& deviceKeyToRemove);

    bool removeDevices(const std::string& deviceKey, const std::vector<std::string>& deviceKeysToRemove);

    std::unique_ptr<std::vector<RegisteredDeviceInformation>> obtainDevices(
      const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType = {}, std::string externalId = {},
      std::chrono::milliseconds timeout = std::chrono::milliseconds{100});

    bool obtainDevicesAsync(const std::string& deviceKey, TimePoint timestampFrom, std::string deviceType = {},
                            std::string externalId = {},
                            std::function<void(const std::vector<RegisteredDeviceInformation>&)> callback = {});

    /**
     * This method allows the user to see the count of error messages a device currently has in the backlog, sent out
     * from the platform.
     *
     * @param deviceKey The key of the device.
     * @return The count of messages held for the device.
     */
    std::uint64_t peekErrorCount(const std::string& deviceKey);

    /**
     * This method allows the user to obtain the first (or the earliest) received message that the backlog of error
     * messages currently holds. This removes the message from the backlog, and moves it to the user.
     *
     * @param deviceKey The key of the device.
     * @return The first message in the backlog held for the device. Can be `nullptr`.
     */
    std::unique_ptr<ErrorMessage> popFrontMessage(const std::string& deviceKey);

    /**
     * This method allows the user to obtain the latest received message that the backlog of error messages
     * currently holds. This removes the message from the backlog, and moves it to the user.
     *
     * @param deviceKey The key of the device.
     * @return The last message in the backlog held for the device. Can be `nullptr`.
     */
    std::unique_ptr<ErrorMessage> popBackMessage(const std::string& deviceKey);

    /**
     * This is the overridden method from the `wolkabout::WolkInterface` interface.
     * This is used to give information about what type of a `WolkInterface` this object is.
     *
     * @return Will always be `WolkInterfaceType::MultiDevice` for objects of this class.
     */
    WolkInterfaceType getType() const override;

private:
    explicit WolkMulti(std::vector<Device> devices);

    bool isDeviceInList(const Device& device);

    bool isDeviceInList(const std::string& deviceKey);

    void reportFilesForDevice(const Device& device);

    void reportFileManagementParametersForDevice(const Device& device);

    void reportFirmwareUpdateForDevice(const Device& device);

    void reportFirmwareUpdateParametersForDevice(const Device& device);

    void notifyConnected() override;

    std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)> wrapRegisterCallback(
      const std::vector<DeviceRegistrationData>& devices,
      const std::function<void(const std::vector<std::string>&, const std::vector<std::string>&)>& callback);

    std::vector<Device> m_devices;
};

template <typename T>
void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference, T value, std::uint64_t rtc)
{
    addReading(deviceKey, reference, legacy::StringUtils::toString(value), rtc);
}

template <typename T>
void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference, const std::vector<T>& values,
                           std::uint64_t rtc)
{
    if (values.empty())
        return;

    std::vector<std::string> stringifiedValues(values.size());
    std::transform(values.cbegin(), values.cend(), stringifiedValues.begin(),
                   [&](const T& value) -> std::string { return legacy::StringUtils::toString(value); });

    addReading(deviceKey, reference, stringifiedValues, rtc);
}
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLK_MULTI_H

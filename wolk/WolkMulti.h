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

#ifndef WOLK_MULTI_H
#define WOLK_MULTI_H

#include "core/utilities/StringUtils.h"
#include "wolk/InboundPlatformMessageHandler.h"
#include "wolk/WolkBuilder.h"
#include "wolk/WolkInterface.h"

#include <algorithm>

namespace wolkabout
{
class WolkMulti final : public WolkInterface
{
    friend class WolkBuilder;

public:
    static WolkBuilder newBuilder(std::vector<Device> devices = {});

    bool addDevice(Device device);

    template <typename T>
    void addReading(const std::string& deviceKey, const std::string& reference, T value, std::uint64_t rtc = 0);

    void addReading(const std::string& deviceKey, const std::string& reference, std::string value,
                    std::uint64_t rtc = 0);

    template <typename T>
    void addReading(const std::string& deviceKey, const std::string& reference, std::initializer_list<T> values,
                    std::uint64_t rtc = 0);

    template <typename T>
    void addReading(const std::string& deviceKey, const std::string& reference, const std::vector<T>& values,
                    std::uint64_t rtc = 0);

    void addReading(const std::string& deviceKey, const std::string& reference, const std::vector<std::string>& values,
                    std::uint64_t rtc = 0);

    void pullFeedValues(const std::string& deviceKey);
    void pullParameters(const std::string& deviceKey);

    void registerFeed(const std::string& deviceKey, const Feed& feed);
    void registerFeeds(const std::string& deviceKey, const std::vector<Feed>& feeds);

    void removeFeed(const std::string& deviceKey, const std::string& reference);
    void removeFeeds(const std::string& deviceKey, const std::vector<std::string>& references);

    void addAttribute(const std::string& deviceKey, Attribute attribute);

    void updateParameter(const std::string& deviceKey, Parameter parameters);

    std::unique_ptr<ErrorMessage> awaitError(const std::string& deviceKey,
                                             std::chrono::milliseconds timeout = std::chrono::milliseconds{100});

    WolkInterfaceType getType() override;

private:
    explicit WolkMulti(std::vector<Device> devices);

    bool isDeviceInList(const Device& device);

    bool isDeviceInList(const std::string& deviceKey);

    void reportFilesForDevice(const Device& device);

    void reportFileManagementParametersForDevice(const Device& device);

    void reportFirmwareUpdateForDevice(const Device& device);

    void reportFirmwareUpdateParametersForDevice(const Device& device);

    void notifyConnected() override;

    std::vector<Device> m_devices;
};

template <typename T>
void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference, T value, std::uint64_t rtc)
{
    addReading(deviceKey, reference, StringUtils::toString(value), rtc);
}

template <typename T>
void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference, std::initializer_list<T> values,
                           std::uint64_t rtc)
{
    if (values.empty())
        return;

    addReading(deviceKey, reference, std::vector<T>(values), rtc);
}

template <typename T>
void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference, const std::vector<T>& values,
                           std::uint64_t rtc)
{
    if (values.empty())
        return;

    std::vector<std::string> stringifiedValues(values.size());
    std::transform(values.cbegin(), values.cend(), stringifiedValues.begin(),
                   [&](const T& value) -> std::string { return StringUtils::toString(value); });

    addReading(deviceKey, reference, stringifiedValues, rtc);
}
}    // namespace wolkabout

#endif    // WOLK_MULTI_H

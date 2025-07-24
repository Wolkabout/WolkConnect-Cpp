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

#ifndef WOLK_SINGLE_H
#define WOLK_SINGLE_H

#include "core/connectivity/ConnectivityService.h"
#include "core/model/Device.h"
#include "core/utilities/StringUtils.h"
#include "wolk/WolkInterface.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace wolkabout
{
namespace connect
{
class WolkBuilder;
/**
 * This is one of the Wolk objects. This is the Wolk object that is meant for a single device.
 * This is more of the default behaviour for the `WolkConnect-Cpp`.
 */
class WolkSingle : public WolkInterface
{
    friend class WolkBuilder;

public:
    /**
     * @brief Initiates wolkabout::WolkBuilder that configures device to connect to Wolkabout IoT Cloud
     * @param device wolkabout::Device
     * @return wolkabout::WolkBuilder instance
     */
    static WolkBuilder newBuilder(Device device);

    /**
     * @brief Publishes sensor reading to Wolkabout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param value Sensor value<br>
     *              Supported types:<br>
     *               - bool<br>
     *               - float<br>
     *               - double<br>
     *               - signed int<br>
     *               - signed long int<br>
     *               - signed long long int<br>
     *               - unsigned int<br>
     *               - unsigned long int<br>
     *               - std::uint64_t<br>
     *               - string<br>
     *               - char*<br>
     *               - const char*<br>
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    template <typename T> void addReading(const std::string& reference, T value, std::uint64_t rtc = 0);

    /**
     * @brief Publishes sensor reading to Wolkabout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param value Sensor value
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addReading(const std::string& reference, std::string value, std::uint64_t rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to Wolkabout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param values Multi-value sensor values<br>
     *              Supported types:<br>
     *               - bool<br>
     *               - float<br>
     *               - double<br>
     *               - signed int<br>
     *               - signed long int<br>
     *               - signed long long int<br>
     *               - unsigned int<br>
     *               - unsigned long int<br>
     *               - std::uint64_t<br>
     *               - string<br>
     *               - char*<br>
     *               - const char*<br>
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    template <typename T>
    void addReading(const std::string& reference, const std::vector<T>& values, std::uint64_t rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to Wolkabout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param values Multi-value sensor values
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addReading(const std::string& reference, const std::vector<std::string>& values, std::uint64_t rtc = 0);

    void addReading(const Reading& reading);

    void addReadings(const std::vector<Reading>& readings);

    void pullFeedValues();
    void pullParameters();

    void obtainDetails(std::function<void(std::vector<std::string>, std::vector<std::string>)> callback);

    void synchronizeParameters(const std::vector<ParameterName>& parameters,
                               std::function<void(std::vector<Parameter>)> callback = nullptr);

    void registerFeed(const Feed& feed);
    void registerFeeds(const std::vector<Feed>& feeds);

    void removeFeed(const std::string& reference);
    void removeFeeds(const std::vector<std::string>& references);

    void addAttribute(Attribute attribute);

    void updateParameter(Parameter parameters);

    void obtainChildren(std::function<void(std::vector<std::string>)> callback);

    WolkInterfaceType getType() const override;

protected:
    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;

    explicit WolkSingle(Device device);

    void notifyConnected() override;

    Device m_device;
};

template <typename T> void WolkSingle::addReading(const std::string& reference, T value, std::uint64_t rtc)
{
    addReading(reference, StringUtils::toString(value), rtc);
}

template <typename T>
void WolkSingle::addReading(const std::string& reference, const std::vector<T>& values, std::uint64_t rtc)
{
    if (values.empty())
        return;

    std::vector<std::string> stringifiedValues(values.size());
    std::transform(values.cbegin(), values.cend(), stringifiedValues.begin(),
                   [&](const T& value) -> std::string { return StringUtils::toString(value); });

    addReading(reference, stringifiedValues, rtc);
}
}    // namespace connect
}    // namespace wolkabout

#endif

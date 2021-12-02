/**
 * Copyright 2021 WolkAbout Technology s.r.o.
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

#ifndef WOLK_H
#define WOLK_H

#include "core/connectivity/ConnectivityService.h"
#include "core/model/Device.h"
#include "core/utilities/CommandBuffer.h"
#include "core/utilities/StringUtils.h"
#include "wolk/WolkBuilder.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class FeedUpdateHandler;
class ParameterHandler;
class ConnectivityService;
class DataProtocol;
class DataService;
class FileManagementService;
class FirmwareUpdateService;
class InboundMessageHandler;
class WolkaboutDataProtocol;

class Wolk
{
    friend class WolkBuilder;

public:
    virtual ~Wolk();

    /**
     * @brief Initiates wolkabout::WolkBuilder that configures device to connect to WolkAbout IoT Cloud
     * @param device wolkabout::Device
     * @return wolkabout::WolkBuilder instance
     */
    static WolkBuilder newBuilder(Device device);

    /**
     * @brief Publishes sensor reading to WolkAbout IoT Cloud<br>
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
     * @brief Publishes sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param value Sensor value
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addReading(const std::string& reference, std::string value, std::uint64_t rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to WolkAbout IoT Cloud<br>
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
    void addReading(const std::string& reference, std::initializer_list<T> values, std::uint64_t rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to WolkAbout IoT Cloud<br>
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
    void addReading(const std::string& reference, const std::vector<T> values, std::uint64_t rtc = 0);

    /**
     * @brief Publishes multi-value sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param values Multi-value sensor values
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addReading(const std::string& reference, const std::vector<std::string> values, std::uint64_t rtc = 0);

    void pullFeedValues();
    void pullParameters();

    void registerFeed(Feed feed);

    void addAttribute(Attribute attribute);

    void updateParameter(Parameter parameters);

    /**
     * @brief Establishes connection with WolkAbout IoT platform
     */
    void connect();

    /**
     * @brief Disconnects from WolkAbout IoT platform
     */
    void disconnect();

    /**
     * @brief Publishes data
     */
    void publish();

private:
    class ConnectivityFacade;

    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;

    Wolk(Device device);

    void addToCommandBuffer(std::function<void()> command);

    static std::uint64_t currentRtc();

    void flushReadings();
    void flushAttributes();
    void flushParameters();

    void handleFeedUpdateCommand(const std::map<std::uint64_t, std::vector<Reading>>& readings);
    void handleParameterCommand(const std::vector<Parameter> parameters);

    void tryConnect(bool firstTime = false);
    void notifyConnected();
    void notifyDisonnected();

    Device m_device;

    std::unique_ptr<DataProtocol> m_dataProtocol;
    std::unique_ptr<FileManagementProtocol> m_fileManagementProtocol;
    std::unique_ptr<FirmwareUpdateProtocol> m_firmwareUpdateProtocol;

    std::unique_ptr<ConnectivityService> m_connectivityService;
    std::shared_ptr<Persistence> m_persistence;

    std::unique_ptr<InboundMessageHandler> m_inboundMessageHandler;

    std::shared_ptr<ConnectivityFacade> m_connectivityManager;

    std::shared_ptr<DataService> m_dataService;
    std::shared_ptr<FileManagementService> m_fileManagementService;
    std::shared_ptr<FirmwareUpdateService> m_firmwareUpdateService;

    std::function<void(const std::map<std::uint64_t, std::vector<Reading>>)> m_feedUpdateHandlerLambda;
    std::weak_ptr<FeedUpdateHandler> m_feedUpdateHandler;

    std::function<void(const std::vector<Parameter>)> m_parameterLambda;
    std::weak_ptr<ParameterHandler> m_parameterHandler;

    std::shared_ptr<FileDownloader> m_fileDownloader;
    std::shared_ptr<FileListener> m_fileListener;

    std::unique_ptr<CommandBuffer> m_commandBuffer;

    class ConnectivityFacade : public ConnectivityServiceListener
    {
    public:
        ConnectivityFacade(InboundMessageHandler& handler, std::function<void()> connectionLostHandler);

        void messageReceived(const std::string& channel, const std::string& message) override;
        void connectionLost() override;
        std::vector<std::string> getChannels() const override;

    private:
        InboundMessageHandler& m_messageHandler;
        std::function<void()> m_connectionLostHandler;
    };
};

template <typename T> void Wolk::addReading(const std::string& reference, T value, std::uint64_t rtc)
{
    addReading(reference, StringUtils::toString(value), rtc);
}

template <typename T>
void Wolk::addReading(const std::string& reference, std::initializer_list<T> values, std::uint64_t rtc)
{
    addReading(reference, std::vector<T>(values), rtc);
}

template <typename T>
void Wolk::addReading(const std::string& reference, const std::vector<T> values, std::uint64_t rtc)
{
    std::vector<std::string> stringifiedValues(values.size());
    std::transform(values.cbegin(), values.cend(), stringifiedValues.begin(),
                   [&](const T& value) -> std::string { return StringUtils::toString(value); });

    addReading(reference, stringifiedValues, rtc);
}

}    // namespace wolkabout

#endif

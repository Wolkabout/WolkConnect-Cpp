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

#include "wolk/WolkMulti.h"

#include "core/utilities/Logger.h"

#include <algorithm>
#include <utility>

namespace wolkabout
{
WolkBuilder WolkMulti::newBuilder(std::vector<Device> devices)
{
    return WolkBuilder(devices);
}

bool WolkMulti::addDevice(Device device)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if there is no device with that key already
    if (isDeviceInList(device))
        return false;

    // Otherwise, add the device
    m_devices.emplace_back(device);
    auto& handler = dynamic_cast<InboundPlatformMessageHandler&>(*m_inboundMessageHandler);
    handler.addDevice(device.getKey());

    // Publish the parameters for the device
    reportFileManagementParametersForDevice(device);
    reportFirmwareUpdateParametersForDevice(device);
    if (m_connected)
    {
        m_dataService->publishParameters(device.getKey());
        reportFilesForDevice(device);
        reportFirmwareUpdateForDevice(device);
    }

    return true;
}

void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference, std::string value,
                           std::uint64_t rtc)
{
    if (rtc == 0)
        rtc = WolkMulti::currentRtc();
    addToCommandBuffer([=]() -> void { m_dataService->addReading(deviceKey, reference, value, rtc); });
}

void WolkMulti::addReading(const std::string& deviceKey, const std::string& reference,
                           const std::vector<std::string>& values, std::uint64_t rtc)
{
    if (rtc == 0)
        rtc = WolkMulti::currentRtc();
    addToCommandBuffer([=]() -> void { m_dataService->addReading(deviceKey, reference, values, rtc); });
}

void WolkMulti::registerFeed(const std::string& deviceKey, const Feed& feed)
{
    addToCommandBuffer([=]() -> void { m_dataService->registerFeed(deviceKey, feed); });
}

void WolkMulti::registerFeeds(const std::string& deviceKey, const std::vector<Feed>& feeds)
{
    addToCommandBuffer([=]() -> void { m_dataService->registerFeeds(deviceKey, feeds); });
}

void WolkMulti::removeFeed(const std::string& deviceKey, const std::string& reference)
{
    addToCommandBuffer([=]() -> void { m_dataService->removeFeed(deviceKey, reference); });
}

void WolkMulti::removeFeeds(const std::string& deviceKey, const std::vector<std::string>& references)
{
    addToCommandBuffer([=]() -> void { m_dataService->removeFeeds(deviceKey, references); });
}

void WolkMulti::pullFeedValues(const std::string& deviceKey)
{
    addToCommandBuffer([=]() -> void { m_dataService->pullFeedValues(deviceKey); });
}

void WolkMulti::pullParameters(const std::string& deviceKey)
{
    addToCommandBuffer([=]() -> void { m_dataService->pullParameters(deviceKey); });
}

void WolkMulti::addAttribute(const std::string& deviceKey, Attribute attribute)
{
    addToCommandBuffer([=]() -> void { m_dataService->addAttribute(deviceKey, attribute); });
}

void WolkMulti::updateParameter(const std::string& deviceKey, Parameter parameter)
{
    addToCommandBuffer([=]() -> void { m_dataService->updateParameter(deviceKey, parameter); });
}

WolkInterfaceType WolkMulti::getType()
{
    return WolkInterfaceType::MultiDevice;
}

WolkMulti::WolkMulti(std::vector<Device> devices) : m_devices(std::move(devices)) {}

bool WolkMulti::isDeviceInList(const Device& device)
{
    return isDeviceInList(device.getKey());
}

bool WolkMulti::isDeviceInList(const std::string& deviceKey)
{
    return std::any_of(m_devices.cbegin(), m_devices.cend(),
                       [&](const Device& device) { return device.getKey() == deviceKey; });
}

void WolkMulti::reportFilesForDevice(const Device& device)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the service can publish such info
    if (m_fileManagementService != nullptr)
    {
        m_fileManagementService->reportPresentFiles(device.getKey());
    }
}

void WolkMulti::reportFileManagementParametersForDevice(const Device& device)
{
    LOG(TRACE) << METHOD_INFO;

    // Make place for the flag values
    auto transferEnabled = false;
    auto urlDownloadEnabled = false;

    // Check if the service can change the results
    if (m_fileManagementService != nullptr)
    {
        transferEnabled = m_fileManagementService->isFileTransferEnabled();
        urlDownloadEnabled = m_fileManagementService->isFileTransferUrlEnabled();
    }

    // And now report the values
    m_dataService->updateParameter(device.getKey(),
                                   {ParameterName::FILE_TRANSFER_PLATFORM_ENABLED, transferEnabled ? "true" : "false"});
    m_dataService->updateParameter(device.getKey(),
                                   {ParameterName::FILE_TRANSFER_URL_ENABLED, urlDownloadEnabled ? "true" : "false"});
}

void WolkMulti::reportFirmwareUpdateForDevice(const Device& device)
{
    LOG(TRACE) << METHOD_INFO;

    if (m_firmwareUpdateService != nullptr)
    {
        if (m_firmwareUpdateService->isInstaller())
        {
            // Load the state
            m_firmwareUpdateService->loadState(device.getKey());

            // Publish everything from the queue
            while (!m_firmwareUpdateService->getQueue().empty())
            {
                // Get the message
                auto message = m_firmwareUpdateService->getQueue().front();
                m_connectivityService->publish(message);
                m_firmwareUpdateService->getQueue().pop();
            }
        }
        else if (m_firmwareUpdateService->isParameterListener())
        {
            m_firmwareUpdateService->obtainParametersAndAnnounce(device.getKey());
        }
    }
}

void WolkMulti::reportFirmwareUpdateParametersForDevice(const Device& device)
{
    LOG(TRACE) << METHOD_INFO;

    // Make place for the parameter values
    auto firmwareVersion = std::string{};

    // Check if the service can change the results
    if (m_firmwareUpdateService != nullptr)
    {
        firmwareVersion = m_firmwareUpdateService->getVersionForDevice(device.getKey());
    }

    // And now report the values
    m_dataService->updateParameter(
      device.getKey(), {ParameterName::FIRMWARE_UPDATE_ENABLED, m_firmwareUpdateService != nullptr ? "true" : "false"});
    m_dataService->updateParameter(device.getKey(), {ParameterName::FIRMWARE_VERSION, firmwareVersion});
}

void WolkMulti::notifyConnected()
{
    WolkInterface::notifyConnected();

    // Report the files and firmware update status for every device
    for (const auto& device : m_devices)
    {
        reportFilesForDevice(device);
        reportFirmwareUpdateForDevice(device);
    }
}
}    // namespace wolkabout

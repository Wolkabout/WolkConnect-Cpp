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

#include "wolk/WolkSingle.h"

#include "wolk/WolkBuilder.h"

namespace wolkabout
{
namespace connect
{
WolkBuilder WolkSingle::newBuilder(Device device)
{
    return WolkBuilder(device);
}

void WolkSingle::addReading(const std::string& reference, std::string value, std::uint64_t rtc)
{
    if (rtc == 0)
    {
        rtc = WolkSingle::currentRtc();
    }

    addToCommandBuffer([=] { m_dataService->addReading(m_device.getKey(), reference, value, rtc); });
}

void WolkSingle::addReading(const std::string& reference, const std::vector<std::string>& values, std::uint64_t rtc)
{
    if (rtc == 0)
    {
        rtc = WolkSingle::currentRtc();
    }

    addToCommandBuffer([=] { m_dataService->addReading(m_device.getKey(), reference, values, rtc); });
}

void WolkSingle::addReading(const Reading& reading)
{
    addToCommandBuffer([this, reading] { m_dataService->addReading(m_device.getKey(), reading); });
}

void WolkSingle::addReadings(const std::vector<Reading>& readings)
{
    addToCommandBuffer([this, readings] { m_dataService->addReadings(m_device.getKey(), readings); });
}

void WolkSingle::pullFeedValues()
{
    addToCommandBuffer([=] { m_dataService->pullFeedValues(m_device.getKey()); });
}

void WolkSingle::pullParameters()
{
    addToCommandBuffer([=] { m_dataService->pullParameters(m_device.getKey()); });
}

void WolkSingle::synchronizeParameters(const std::vector<ParameterName>& parameters,
                                       std::function<void(std::vector<Parameter>)> callback)
{
    addToCommandBuffer([=] { m_dataService->synchronizeParameters(m_device.getKey(), parameters, callback); });
}

void WolkSingle::registerFeed(const Feed& feed)
{
    addToCommandBuffer([=] { m_dataService->registerFeed(m_device.getKey(), feed); });
}

void WolkSingle::registerFeeds(const std::vector<Feed>& feeds)
{
    addToCommandBuffer([=] { m_dataService->registerFeeds(m_device.getKey(), feeds); });
}

void WolkSingle::removeFeed(const std::string& reference)
{
    addToCommandBuffer([=] { m_dataService->removeFeed(m_device.getKey(), reference); });
}

void WolkSingle::removeFeeds(const std::vector<std::string>& references)
{
    addToCommandBuffer([=] { m_dataService->removeFeeds(m_device.getKey(), references); });
}

void WolkSingle::addAttribute(Attribute attribute)
{
    addToCommandBuffer([=] { m_dataService->addAttribute(m_device.getKey(), attribute); });
}

void WolkSingle::updateParameter(Parameter parameter)
{
    addToCommandBuffer([=] { m_dataService->updateParameter(m_device.getKey(), parameter); });
}

std::unique_ptr<ErrorMessage> WolkSingle::awaitError(std::chrono::milliseconds timeout)
{
    return m_errorService->obtainOrAwaitMessageForDevice(m_device.getKey(), timeout);
}

WolkInterfaceType WolkSingle::getType()
{
    return WolkInterfaceType::SingleDevice;
}

WolkSingle::WolkSingle(Device device) : m_device(std::move(device)) {}

void WolkSingle::notifyConnected()
{
    WolkInterface::notifyConnected();

    if (m_fileManagementService != nullptr)
    {
        m_fileManagementService->reportPresentFiles(m_device.getKey());
    }

    if (m_firmwareUpdateService != nullptr)
    {
        if (m_firmwareUpdateService->isInstaller())
        {
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
            m_firmwareUpdateService->obtainParametersAndAnnounce(m_device.getKey());
        }
    }
}
}    // namespace connect
}    // namespace wolkabout

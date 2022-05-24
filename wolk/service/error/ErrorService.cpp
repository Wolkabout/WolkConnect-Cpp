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

#include "wolk/service/error/ErrorService.h"

#include "core/utilities/Logger.h"

namespace wolkabout
{
namespace connect
{
const std::chrono::milliseconds TIMER_PERIOD = std::chrono::milliseconds{10};

ErrorService::ErrorService(ErrorProtocol& protocol, std::chrono::milliseconds retainTime)
: m_protocol(protocol), m_working(true), m_retainTime(std::move(retainTime))
{
}

ErrorService::~ErrorService()
{
    stop();

    // Also stop all the condition variables
    std::lock_guard<std::mutex> lock{m_cvMutex};
    for (const auto& device : m_conditionVariables)
        device.second->notify_all();
}

void ErrorService::start()
{
    LOG(TRACE) << METHOD_INFO;
    m_timer.run(TIMER_PERIOD, [&] { timerRuntime(); });
}

void ErrorService::stop()
{
    LOG(TRACE) << METHOD_INFO;
    m_timer.stop();
}

std::uint64_t ErrorService::peekMessagesForDevice(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Lock to prevent any race conditions
    std::lock_guard<std::mutex> lock{m_cacheMutex};

    // Check whether there is a map entry in cache for the device at all
    const auto deviceMessagesIt = m_cached.find(deviceKey);
    if (deviceMessagesIt == m_cached.cend())
        return 0;
    return deviceMessagesIt->second.size();
}

std::unique_ptr<ErrorMessage> ErrorService::obtainFirstMessageForDevice(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Lock to prevent any race conditions
    std::lock_guard<std::mutex> lock{m_cacheMutex};

    // Check whether there is a map entry in cache for the device at all
    const auto deviceMessagesIt = m_cached.find(deviceKey);
    if (deviceMessagesIt == m_cached.cend())
        return nullptr;
    auto& deviceMessages = deviceMessagesIt->second;
    if (deviceMessages.empty())
        return nullptr;

    // Make place for the message, and remove it from the map
    auto message = std::unique_ptr<ErrorMessage>{deviceMessages.begin()->second.release()};
    deviceMessages.erase(deviceMessages.begin());
    return message;
}

std::unique_ptr<ErrorMessage> ErrorService::obtainLastMessageForDevice(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Lock to prevent any race conditions
    std::lock_guard<std::mutex> lock{m_cacheMutex};

    // Check whether there is a map entry in cache for the device at all
    const auto deviceMessagesIt = m_cached.find(deviceKey);
    if (deviceMessagesIt == m_cached.cend())
        return nullptr;
    auto& deviceMessages = deviceMessagesIt->second;
    if (deviceMessages.empty())
        return nullptr;

    // Make place for the message, and remove it from the map
    auto lastIterator = deviceMessages.end();
    --lastIterator;
    auto message = std::unique_ptr<ErrorMessage>{lastIterator->second.release()};
    deviceMessages.erase(lastIterator);
    return message;
}

bool ErrorService::awaitMessage(const std::string& deviceKey, std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;

    // Make the lock for the mutex and start waiting
    auto start = peekMessagesForDevice(deviceKey);
    {
        std::lock_guard<std::mutex> lock{m_cvMutex};
        if (m_mutexes.find(deviceKey) == m_mutexes.cend())
            m_mutexes.emplace(deviceKey, std::unique_ptr<std::mutex>{new std::mutex});
    }
    auto deviceLock = std::unique_lock<std::mutex>{*m_mutexes[deviceKey]};
    {
        std::lock_guard<std::mutex> lock{m_cvMutex};
        if (m_conditionVariables.find(deviceKey) == m_conditionVariables.cend())
            m_conditionVariables.emplace(deviceKey,
                                         std::unique_ptr<std::condition_variable>{new std::condition_variable});
    }

    // Check whether the count of messages changed
    auto different = false;
    m_conditionVariables[deviceKey]->wait_for(deviceLock, timeout, [&] {
        different = peekMessagesForDevice(deviceKey) != start;
        return !m_working || different;
    });
    return different ? different : peekMessagesForDevice(deviceKey) != start;
}

std::unique_ptr<ErrorMessage> ErrorService::obtainOrAwaitMessageForDevice(const std::string& deviceKey,
                                                                          std::chrono::milliseconds timeout)
{
    LOG(TRACE) << METHOD_INFO;

    // Check the cache first
    auto cacheResult = obtainFirstMessageForDevice(deviceKey);
    if (cacheResult != nullptr)
        return cacheResult;
    if (awaitMessage(deviceKey, timeout))
        return obtainFirstMessageForDevice(deviceKey);
    else
        return nullptr;
}

void ErrorService::messageReceived(std::shared_ptr<Message> message)
{
    LOG(TRACE) << METHOD_INFO;

    // Try to parse the message
    auto errorMessage = m_protocol.parseError(message);
    if (errorMessage == nullptr)
    {
        LOG(ERROR) << "Failed to parse incoming 'ErrorMessage' - Not a valid 'ErrorMessage'.";
        return;
    }
    const auto deviceKey = errorMessage->getDeviceKey();    // DO NOT TAKE A REFERENCE HERE! We need it to be a copy
    LOG(DEBUG) << "Received 'ErrorMessage' for device '" << deviceKey << "' -> '" << errorMessage->getMessage() << "'.";

    // Add the message into the cache and notify the condition variable
    {
        std::lock_guard<std::mutex> lock{m_cacheMutex};
        if (m_cached.find(deviceKey) == m_cached.cend())
            m_cached.emplace(deviceKey, DeviceErrorMessages{});
        m_cached[deviceKey].emplace(errorMessage->getArrivalTime(), std::move(errorMessage));
    }

    // Find out if there's a condition variable and notify it
    {
        std::lock_guard<std::mutex> lock{m_cvMutex};
        if (m_conditionVariables.find(deviceKey) != m_conditionVariables.cend())
            m_conditionVariables[deviceKey]->notify_one();
    }
}

const Protocol& ErrorService::getProtocol()
{
    return m_protocol;
}

void ErrorService::timerRuntime()
{
    // Lock the mutex, check all the messages.
    std::lock_guard<std::mutex> lock{m_cacheMutex};
    const auto now = std::chrono::system_clock::now();
    for (auto& deviceErrors : m_cached)
    {
        // Collect all the iterators that need to be removed
        auto& messages = deviceErrors.second;
        for (auto i = messages.size(); i > 0; --i)
        {
            auto it = messages.begin();
            std::advance(it, i);
            if (now - it->first >= m_retainTime)
            {
                LOG(TRACE) << "Removing a cached message for device '" << deviceErrors.first << "'.";
                messages.erase(messages.begin(), it);
            }
        }
    }
}
}    // namespace connect
}    // namespace wolkabout

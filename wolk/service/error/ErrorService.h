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

#ifndef WOLKABOUTCONNECTOR_ERRORSERVICE_H
#define WOLKABOUTCONNECTOR_ERRORSERVICE_H

#include "core/MessageListener.h"
#include "core/protocol/ErrorProtocol.h"
#include "core/utilities/Service.h"
#include "core/utilities/Timer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>

namespace wolkabout
{
namespace connect
{
// Type alias for the metadata of an ErrorMessage.
using TimePoint = std::chrono::system_clock::time_point;
using DeviceErrorMessages = std::map<TimePoint, std::unique_ptr<ErrorMessage>>;
using ErrorMessageCache = std::map<std::string, DeviceErrorMessages>;

/**
 * This is the service that will receive ErrorMessages. This service can both retain some error messages, and also await
 * error messages if necessary. This should be mostly used by other services that receive their errors through the error
 * topic.
 */
class ErrorService : public MessageListener, public Service
{
public:
    /**
     * Default parameter constructor.
     *
     * @param protocol The protocol which the ErrorService will follow.
     * @param retainTime The time that defines how long will the ErrorService retain an ErrorMessage.
     */
    explicit ErrorService(ErrorProtocol& protocol,
                          std::chrono::milliseconds retainTime = std::chrono::milliseconds{500});

    /**
     * Overridden destructor that will stop the running timer.
     */
    ~ErrorService() override;

    /**
     * Method that will start the caching timer.
     */
    void start() override;

    /**
     * Method that will start the caching timer.
     */
    void stop() override;

    /**
     * This method is used to check whether the cache currently is at all holding an error message for a device.
     *
     * @param deviceKey The device key for which the cache is checked.
     * @return Whether the cache is holding at least one message for the device.
     */
    virtual std::uint64_t peekMessagesForDevice(const std::string& deviceKey);

    /**
     * This method is used to force obtain the first message from the cache of the device.
     *
     * @param deviceKey The device key for which the cache is checked.
     * @return A pointer to the ErrorMessage from cache. A nullptr if no messages for the device are in cache.
     */
    virtual std::unique_ptr<ErrorMessage> obtainFirstMessageForDevice(const std::string& deviceKey);

    /**
     * This method is used to force obtain the last message from the cache of the device.
     *
     * @param deviceKey The device key for which a message is being obtained.
     * @return A pointer to the ErrorMessage from cache. A nullptr if not messages for the device are in cache.
     */
    virtual std::unique_ptr<ErrorMessage> obtainLastMessageForDevice(const std::string& deviceKey);

    /**
     * This method is used to await a message to be added into the cache for a device key.
     * If a message has been added into the cache, it will be returned.
     *
     * @param deviceKey The device key for which the message is awaited.
     * @param timeout The maximum time the message will be waited for.
     * @return A pointer to the ErrorMessage that has arrived. A nullptr if no messages arrived for the device.
     */
    virtual bool awaitMessage(const std::string& deviceKey, std::chrono::milliseconds timeout);

    /**
     * This is the method that other services can use to check or await an ErrorMessage. This will first check if an
     * ErrorMessage for the device has already been cached in the retain time.
     *
     * @param deviceKey The device key for which an error message is expected.
     * @param timeout The time for how long will we max wait for the message.
     * @return A pointer to an ErrorMessage if a message for the device key has been found in the cache, or has been
     * awaited. Nullptr if neither.
     */
    virtual std::unique_ptr<ErrorMessage> obtainOrAwaitMessageForDevice(const std::string& deviceKey,
                                                                        std::chrono::milliseconds timeout);

    /**
     * This is the overridden method from the `MessageListener` interface.
     * This is the method that will receive messages from MQTT.
     *
     * @param message The received message that should be parsed.
     */
    void messageReceived(std::shared_ptr<Message> message) override;

    /**
     * This is the overridden method from the `MessageListener` interface.
     * This is a getter for the protocol this service wants to use.
     *
     * @return The reference to the protocol used by the service.
     */
    const Protocol& getProtocol() override;

private:
    /**
     * This is the internal method that is invoked by the timer to check whether any cached messages have expired.
     */
    void timerRuntime();

    // This is where we store the protocol reference
    ErrorProtocol& m_protocol;

    // Here we store cached error messages
    bool m_working;
    Timer m_timer;
    std::chrono::milliseconds m_retainTime;
    std::mutex m_cacheMutex;
    ErrorMessageCache m_cached;

    // This is the data necessary for listening to errors. The iterator serves to make everyone a unique subscription
    // id, and the map where subscriptions are stored, key being the device key.
    std::mutex m_cvMutex;
    std::map<std::string, std::unique_ptr<std::mutex>> m_mutexes;
    std::map<std::string, std::unique_ptr<std::condition_variable>> m_conditionVariables;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_ERRORSERVICE_H

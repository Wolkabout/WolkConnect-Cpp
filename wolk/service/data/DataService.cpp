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

#include "wolk/service/data/DataService.h"

#include "core/Types.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/connectivity/OutboundRetryMessageHandler.h"
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/model/Message.h"
#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/utilities/Logger.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace
{
const std::uint16_t RETRY_COUNT = 3;
const std::chrono::milliseconds RETRY_TIMEOUT{5000};
}    // namespace

namespace wolkabout
{
namespace connect
{
const std::string DataService::PERSISTENCE_KEY_DELIMITER = "+";

DataService::DataService(DataProtocol& protocol, Persistence& persistence, ConnectivityService& connectivityService,
                         OutboundRetryMessageHandler& outboundRetryMessageHandler,
                         FeedUpdateSetHandler feedUpdateHandler, ParameterSyncHandler parameterSyncHandler,
                         DetailsSyncHandler detailsSyncHandler)
: m_protocol{protocol}
, m_persistence{persistence}
, m_connectivityService{connectivityService}
, m_outboundRetryMessageHandler{outboundRetryMessageHandler}
, m_feedUpdateHandler{std::move(feedUpdateHandler)}
, m_parameterSyncHandler{std::move(parameterSyncHandler)}
, m_detailsSyncHandler{std::move(detailsSyncHandler)}
, m_iterator(0)
{
}

void DataService::addReading(const std::string& deviceKey, const std::string& reference, const std::string& value,
                             std::uint64_t rtc)
{
    m_persistence.putReading(makePersistenceKey(deviceKey, reference), Reading{reference, value, rtc});
}

void DataService::addReading(const std::string& deviceKey, const std::string& reference,
                             const std::vector<std::string>& value, std::uint64_t rtc)
{
    m_persistence.putReading(makePersistenceKey(deviceKey, reference), Reading{reference, value, rtc});
}

void DataService::addReading(const std::string& deviceKey, const Reading& reading)
{
    m_persistence.putReading(makePersistenceKey(deviceKey, reading.getReference()), reading);
}

void DataService::addReadings(const std::string& deviceKey, const std::vector<Reading>& readings)
{
    for (const auto& reading : readings)
        m_persistence.putReading(makePersistenceKey(deviceKey, reading.getReference()), reading);
}

void DataService::addAttribute(const std::string& deviceKey, const Attribute& attribute)
{
    m_persistence.putAttribute(makePersistenceKey(deviceKey, attribute.getName()),
                               std::make_shared<Attribute>(attribute));
}

void DataService::updateParameter(const std::string& deviceKey, const Parameter& parameter)
{
    m_persistence.putParameter(makePersistenceKey(deviceKey, toString(parameter.first)), parameter);
}

void DataService::registerFeed(const std::string& deviceKey, Feed feed)
{
    registerFeeds(deviceKey, {std::move(feed)});
}

void DataService::registerFeeds(const std::string& deviceKey, std::vector<Feed> feeds)
{
    LOG(TRACE) << METHOD_INFO;
    auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, FeedRegistrationMessage(std::move(feeds)))};
    if (message == nullptr)
        LOG(ERROR) << "Failed to register feeds -> Failed to parse the outgoing 'FeedRegistrationMessage'.";
    else if (!m_connectivityService.publish(message))
        LOG(ERROR) << "Failed to register feeds -> Failed to publish the outgoing 'FeedRegistrationMessage'.";
}

void DataService::removeFeed(const std::string& deviceKey, std::string reference)
{
    removeFeeds(deviceKey, {std::move(reference)});
}

void DataService::removeFeeds(const std::string& deviceKey, std::vector<std::string> feeds)
{
    LOG(TRACE) << METHOD_INFO;
    auto message =
      std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, FeedRemovalMessage(std::move(feeds))));
    if (message == nullptr)
        LOG(ERROR) << "Failed to remove feeds -> Failed to parse the outgoing 'FeedRemovalMessage'.";
    else if (!m_connectivityService.publish(message))
        LOG(ERROR) << "Failed to remove feeds -> Failed to publish the outgoing 'FeedRemovalMessage'.";
}

void DataService::pullFeedValues(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;
    auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, PullFeedValuesMessage{})};
    if (message == nullptr)
        LOG(ERROR) << "Failed to pull feed values -> Failed to parse the outgoing 'PullFeedValuesMessage.";
    else if (!m_connectivityService.publish(message))
        LOG(ERROR) << "Failed to pull feed values -> Failed to publish the outgoing 'PullFeedValuesMessage.";
}

void DataService::pullParameters(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;
    auto message = std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, ParametersPullMessage{})};
    if (message == nullptr)
        LOG(ERROR) << "Failed to pull parameter values -> Failed to parse the outgoing 'ParametersPullMessage.";
    else if (!m_connectivityService.publish(message))
        LOG(ERROR) << "Failed to pull parameter values -> Failed to publish the outgoing 'ParametersPullMessage.";
}

bool DataService::synchronizeParameters(const std::string& deviceKey, const std::vector<ParameterName>& parameters,
                                        std::function<void(std::vector<Parameter>)> callback)
{
    LOG(TRACE) << METHOD_INFO;

    // Make the message for synchronization
    auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, SynchronizeParametersMessage{parameters})};
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to synchronize parameters - Failed to parse outgoing SynchronizeParametersMessage.";
        return false;
    }

    // Send the message out, and add the subscription to the map.
    if (!m_connectivityService.publish(message))
    {
        LOG(ERROR) << "Failed to synchronize parameters - Failed to send the outgoing SynchronizeParameterMessage.";
        return false;
    }
    if (callback)
    {
        std::lock_guard<std::mutex> lockGuard{m_subscriptionMutex};
        m_parameterSubscriptions.emplace(m_iterator++, ParameterSubscription{parameters, std::move(callback)});
    }
    return true;
}

bool DataService::detailsSynchronizationAsync(
  const std::string& deviceKey, std::function<void(std::vector<std::string>, std::vector<std::string>)> callback)
{
    LOG(TRACE) << METHOD_INFO;
    const auto errorPrefix = "Failed to synchronize device details";

    // Make the request message for synchronization
    auto message =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, DetailsSynchronizationRequestMessage{})};
    if (message == nullptr)
    {
        LOG(ERROR) << errorPrefix << " -> Failed to parse outgoing 'DetailsSynchronizationRequestMessage'.";
        return false;
    }

    // Send the message out and add the callback in the map
    m_outboundRetryMessageHandler.addMessage(
      {message, m_protocol.getResponseChannelForMessage(MessageType::DETAILS_SYNCHRONIZATION_REQUEST, deviceKey),
       [](const std::shared_ptr<Message>&) {
           LOG(ERROR)
             << "Failed to receive response for 'DetailsSynchronizationRequestMessage' - no response from platform.";
       },
       RETRY_COUNT, RETRY_TIMEOUT});
    if (callback)
    {
        std::lock_guard<std::mutex> lock{m_detailsMutex};
        m_detailsCallbacks.push(std::move(callback));
    }
    return true;
}

void DataService::publishReadings()
{
    for (const auto& key : m_persistence.getReadingsKeys())
    {
        publishReadingsForPersistenceKey(key);
    }
}

void DataService::publishReadings(const std::string& deviceKey)
{
    publishReadingsForPersistenceKey(deviceKey);
}

void DataService::publishAttributes()
{
    LOG(TRACE) << METHOD_INFO;

    // Extract all attributes for all devices and group them up by device
    auto attributes = std::map<std::string, std::vector<Attribute>>{};
    for (const auto& attributeFromPersistence : m_persistence.getAttributes())
    {
        // Extract everything about the attribute
        auto deviceKey = std::string{};
        auto reference = std::string{};
        std::tie(deviceKey, reference) = parsePersistenceKey(attributeFromPersistence.first);

        // Check if there is already an array for the device
        auto it = attributes.find(deviceKey);
        if (it == attributes.cend())
            it = attributes.emplace(deviceKey, std::vector<Attribute>{}).first;
        it->second.emplace_back(*attributeFromPersistence.second);
    }
    if (attributes.empty())
        return;

    // Send a message out for all devices
    for (const auto& deviceAttributes : attributes)
    {
        // Make a lambda that will delete all these attributes from persistence
        const auto& deviceKey = deviceAttributes.first;
        auto deleteAllAttributes = [&]() {
            for (const auto& attribute : deviceAttributes.second)
                m_persistence.removeAttributes(makePersistenceKey(deviceKey, attribute.getName()));
        };

        // Form the message
        auto outboundMessage = std::shared_ptr<Message>(
          m_protocol.makeOutboundMessage(deviceKey, AttributeRegistrationMessage(deviceAttributes.second)));
        if (!outboundMessage)
        {
            LOG(ERROR) << "Unable to create message from attributes";
            deleteAllAttributes();
            return;
        }
        if (m_connectivityService.publish(outboundMessage))
            deleteAllAttributes();
    }
}

void DataService::publishAttributes(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Extract all the attributes for this device key
    auto attributes = std::vector<Attribute>{};
    for (const auto& attribute : m_persistence.getAttributes())
    {
        // Check the device key
        auto attributeDeviceKey = std::string{};
        auto reference = std::string{};
        std::tie(attributeDeviceKey, reference) = parsePersistenceKey(attribute.first);
        if (attributeDeviceKey == deviceKey)
            attributes.emplace_back(*attribute.second);
    }
    if (attributes.empty())
        return;

    // Make a lambda that will delete all these attributes from persistence
    auto deleteAllAttributes = [&]() {
        for (const auto& attribute : attributes)
            m_persistence.removeAttributes(makePersistenceKey(deviceKey, attribute.getName()));
    };

    // Form the message
    auto message = AttributeRegistrationMessage(attributes);
    auto outboundMessage = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, message));
    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from attributes";
        deleteAllAttributes();
        return;
    }
    if (m_connectivityService.publish(outboundMessage))
        deleteAllAttributes();
}

void DataService::publishParameters()
{
    LOG(TRACE) << METHOD_INFO;

    // Extract all attributes for all devices and group them up by device
    auto parameters = std::map<std::string, std::vector<Parameter>>{};
    for (const auto& parameterFromPersistence : m_persistence.getParameters())
    {
        // Extract everything about the parameter
        auto deviceKey = std::string{};
        auto reference = std::string{};
        std::tie(deviceKey, reference) = parsePersistenceKey(parameterFromPersistence.first);

        // Check if there is already an array for the device
        auto it = parameters.find(deviceKey);
        if (it == parameters.cend())
            it = parameters.emplace(deviceKey, std::vector<Parameter>{}).first;
        it->second.emplace_back(parameterFromPersistence.second);
    }
    if (parameters.empty())
        return;

    // Send a message out for all devices
    for (const auto& deviceParameters : parameters)
    {
        // Make a lambda that will delete all these parameters from persistence
        const auto& deviceKey = deviceParameters.first;
        auto deleteAllParameters = [&]() {
            for (const auto& parameter : deviceParameters.second)
                m_persistence.removeParameters(makePersistenceKey(deviceKey, toString(parameter.first)));
        };

        // Form the message
        auto message = ParametersUpdateMessage(deviceParameters.second);
        auto outboundMessage = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, message));
        if (!outboundMessage)
        {
            LOG(ERROR) << "Unable to create message from parameters";
            deleteAllParameters();
            return;
        }
        if (m_connectivityService.publish(outboundMessage))
            deleteAllParameters();
    }
}

void DataService::publishParameters(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Extract all the attributes for this device key
    auto parameters = std::vector<Parameter>{};
    for (const auto& parameter : m_persistence.getParameters())
    {
        // Check the device key
        auto parameterDeviceKey = std::string{};
        auto reference = std::string{};
        std::tie(parameterDeviceKey, reference) = parsePersistenceKey(parameter.first);
        if (parameterDeviceKey == deviceKey)
            parameters.emplace_back(parameter.second);
    }
    if (parameters.empty())
        return;

    // Make a lambda that will delete all these parameters from persistence
    auto deleteAllParameters = [&]() {
        for (const auto& parameter : parameters)
            m_persistence.removeParameters(makePersistenceKey(deviceKey, toString(parameter.first)));
    };

    // Form the message
    auto message = ParametersUpdateMessage(parameters);
    auto outboundMessage = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, message));
    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from parameters";
        deleteAllParameters();
        return;
    }
    if (m_connectivityService.publish(outboundMessage))
        deleteAllParameters();
}

const Protocol& DataService::getProtocol()
{
    return m_protocol;
}

void DataService::messageReceived(std::shared_ptr<Message> message)
{
    LOG(TRACE) << METHOD_INFO;
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to handle message - The message is null!";
        return;
    }
    const auto deviceKey = m_protocol.getDeviceKey(*message);
    if (deviceKey.empty())
    {
        LOG(WARN) << "Unable to extract device key from channel: " << message->getChannel();
        return;
    }

    switch (m_protocol.getMessageType(*message))
    {
    case MessageType::FEED_VALUES:
    {
        auto feedValuesMessage = m_protocol.parseFeedValues(message);
        if (feedValuesMessage == nullptr)
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        else if (m_feedUpdateHandler)
            m_feedUpdateHandler(deviceKey, feedValuesMessage->getReadings());
        return;
    }
    case MessageType::PARAMETER_SYNC:
    {
        auto parameterMessage = m_protocol.parseParameters(message);
        if (parameterMessage == nullptr)
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        else if (checkIfSubscriptionIsWaiting(parameterMessage))    // It's important to first check this
            return;
        else if (m_parameterSyncHandler)
            m_parameterSyncHandler(deviceKey, parameterMessage->getParameters());
        return;
    }
    case MessageType::DETAILS_SYNCHRONIZATION_RESPONSE:
    {
        m_outboundRetryMessageHandler.messageReceived(message);
        auto detailsSynchronization = m_protocol.parseDetails(message);
        if (detailsSynchronization == nullptr)
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        else if (checkIfCallbackIsWaiting(detailsSynchronization))
            return;
        else if (m_detailsSyncHandler)
            m_detailsSyncHandler(deviceKey, detailsSynchronization->getFeeds(),
                                 detailsSynchronization->getAttributes());
        return;
    }
    default:
    {
        LOG(WARN) << "Unable to parse message channel: " << message->getChannel();
    }
    }
}

std::string DataService::makePersistenceKey(const std::string& deviceKey, const std::string& reference)
{
    return deviceKey + PERSISTENCE_KEY_DELIMITER + reference;
}

std::pair<std::string, std::string> DataService::parsePersistenceKey(const std::string& key)
{
    auto pos = key.find(PERSISTENCE_KEY_DELIMITER);
    if (pos == std::string::npos)
    {
        return {"", ""};
    }

    auto deviceKey = key.substr(0, pos);
    auto reference = key.substr(pos + PERSISTENCE_KEY_DELIMITER.size(), std::string::npos);
    return {deviceKey, reference};
}

bool DataService::checkIfSubscriptionIsWaiting(const std::shared_ptr<ParametersUpdateMessage>& parameterMessage)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if there's a subscription waiting for those parameters
    {
        std::lock_guard<std::mutex> lockGuard{m_subscriptionMutex};
        for (const auto& subscription : m_parameterSubscriptions)
        {
            // Check if the list of parameters is the same length, and then content
            const auto& parameters = subscription.second.parameters;
            const auto& values = parameterMessage->getParameters();
            if (parameterMessage->getParameters().size() != parameters.size())
            {
                continue;
            }
            auto allNamesMatching = std::all_of(parameters.cbegin(), parameters.cend(), [&](const ParameterName& name) {
                return std::find_if(values.begin(), values.end(), [&](const Parameter& parameter) {
                           return parameter.first == name;
                       }) != values.cend();
            });
            if (!allNamesMatching)
            {
                continue;
            }

            // Then we found the ones for the subscription!
            const auto& callback = subscription.second.callback;
            if (callback)
                m_commandBuffer.pushCommand(
                  std::make_shared<std::function<void()>>([callback, values]() { callback(values); }));

            // And we can clear the subscription
            m_parameterSubscriptions.erase(subscription.first);
            return callback != nullptr;
        }
    }
    return false;
}

bool DataService::checkIfCallbackIsWaiting(
  const std::shared_ptr<DetailsSynchronizationResponseMessage>& synchronizationResponseMessage)
{
    LOG(TRACE) << METHOD_INFO;

    if (synchronizationResponseMessage == nullptr)
        return false;

    {
        std::lock_guard<std::mutex> lock{m_detailsMutex};
        if (!m_detailsCallbacks.empty())
        {
            const auto callback = m_detailsCallbacks.front();
            m_commandBuffer.pushCommand(
              std::make_shared<std::function<void()>>([callback, synchronizationResponseMessage] {
                  callback(synchronizationResponseMessage->getFeeds(), synchronizationResponseMessage->getAttributes());
              }));
            m_detailsCallbacks.pop();
            return true;
        }
    }
    return false;
}

void DataService::publishReadingsForPersistenceKey(const std::string& persistenceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Read all information from persistence
    auto readings = std::vector<Reading>{};
    for (const auto& readingFromPersistence : m_persistence.getReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT))
        readings.emplace_back(*readingFromPersistence);
    if (readings.empty())
        return;
    auto deviceKey = std::string{};
    auto reference = std::string{};
    std::tie(deviceKey, reference) = parsePersistenceKey(persistenceKey);

    // Check the device key and the reference
    if (deviceKey.empty())
    {
        LOG(ERROR) << "Unable to create message from readings: The device key is empty.";
        return;
    }
    // Create the message
    const auto outboundMessage =
      std::shared_ptr<Message>{m_protocol.makeOutboundMessage(deviceKey, FeedValuesMessage{readings})};
    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from readings: " << persistenceKey;
        m_persistence.removeReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);
        return;
    }
    if (m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);
        publishReadingsForPersistenceKey(persistenceKey);
    }
}
}    // namespace connect
}    // namespace wolkabout

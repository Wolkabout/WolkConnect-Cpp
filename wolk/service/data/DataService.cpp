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
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/model/Message.h"
#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/utilities/Logger.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace wolkabout
{
const std::string DataService::PERSISTENCE_KEY_DELIMITER = "+";

DataService::DataService(std::string deviceKey, DataProtocol& protocol, Persistence& persistence,
                         ConnectivityService& connectivityService, FeedUpdateSetHandler feedUpdateHandler,
                         ParameterSyncHandler parameterSyncHandler)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_persistence{persistence}
, m_connectivityService{connectivityService}
, m_feedUpdateHandler{std::move(feedUpdateHandler)}
, m_parameterSyncHandler{std::move(parameterSyncHandler)}
, m_iterator(0)
{
}

void DataService::addReading(const std::string& reference, const std::string& value, std::uint64_t rtc)
{
    auto reading = std::make_shared<Reading>(reference, value, rtc);
    m_persistence.putReading(reference, reading);
}

void DataService::addReading(const std::string& reference, const std::vector<std::string>& value, std::uint64_t rtc)
{
    auto reading = std::make_shared<Reading>(reference, value, rtc);
    m_persistence.putReading(reference, reading);
}

void DataService::addAttribute(const Attribute& attribute)
{
    auto attr = std::make_shared<Attribute>(attribute);

    m_persistence.putAttribute(attr);
}

void DataService::updateParameter(Parameter parameter)
{
    m_persistence.putParameter(std::move(parameter));
}

void DataService::registerFeed(Feed feed)
{
    registerFeeds({std::move(feed)});
}

void DataService::registerFeeds(std::vector<Feed> feeds)
{
    FeedRegistrationMessage feedRegistrationMessage(std::move(feeds));

    const std::shared_ptr<Message> outboundMessage =
      m_protocol.makeOutboundMessage(m_deviceKey, feedRegistrationMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create feed registration message from feed";
        return;
    }

    if (!m_connectivityService.publish(outboundMessage))
    {
        LOG(ERROR) << "Unable to publish feed registration message";
    }
}

void DataService::removeFeed(std::string reference)
{
    removeFeeds({std::move(reference)});
}

void DataService::removeFeeds(std::vector<std::string> feeds)
{
    LOG(TRACE) << METHOD_INFO;

    auto removal = FeedRemovalMessage(std::move(feeds));
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(m_deviceKey, removal));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to remove feeds -> Failed to parse the outgoing 'FeedRemovalMessage'.";
        return;
    }
    if (!m_connectivityService.publish(message))
    {
        LOG(ERROR) << "Failed to remove feeds -> Failed to publish the outgoing 'FeedRemovalMessage'.";
        return;
    }
}

void DataService::pullFeedValues()
{
    PullFeedValuesMessage pullFeedValuesMessage;

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeOutboundMessage(m_deviceKey, pullFeedValuesMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create Pull Feeds Value message from feed";
        return;
    }

    if (!m_connectivityService.publish(outboundMessage))
    {
        LOG(ERROR) << "Unable to publish Pull Feeds Value message";
    }
}

void DataService::pullParameters()
{
    ParametersPullMessage parametersPullMessage;

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeOutboundMessage(m_deviceKey, parametersPullMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create Pull Parameters message from feed";
        return;
    }

    if (!m_connectivityService.publish(outboundMessage))
    {
        LOG(ERROR) << "Unable to publish Pull Parameters message";
    }
}

bool DataService::synchronizeParameters(const std::vector<ParameterName>& parameters,
                                        std::function<void(std::vector<Parameter>)> callback)
{
    LOG(TRACE) << METHOD_INFO;

    // Make the subscription object in the map
    auto subscription = ParameterSubscription{parameters, std::move(callback)};

    // Make the message for synchronization
    auto synchronization = SynchronizeParametersMessage{parameters};
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(m_deviceKey, synchronization));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to synchronize parameters - Failed to parse outgoing SynchronizeParametersMessage.";
        return false;
    }

    // Lock the subscription mutex, send the message out, and add the subscription to the map.
    {
        std::lock_guard<std::mutex> lockGuard{m_subscriptionMutex};
        if (!m_connectivityService.publish(message))
        {
            LOG(ERROR) << "Failed to synchronize parameters - Failed to send the outgoing SynchronizeParameterMessage.";
            return false;
        }
        m_parameterSubscriptions.emplace(m_iterator++, std::move(subscription));
        return true;
    }
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
    auto attributes = std::vector<Attribute>{};
    for (const auto& attributeFromPersistence : m_persistence.getAttributes())
        attributes.emplace_back(*attributeFromPersistence);
    if (attributes.empty())
        return;

    AttributeRegistrationMessage attributeRegistrationMessage(attributes);

    const std::shared_ptr<Message> outboundMessage =
      m_protocol.makeOutboundMessage(m_deviceKey, attributeRegistrationMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from attributes";
        m_persistence.removeAttributes();
        return;
    }

    if (m_connectivityService.publish(outboundMessage))
        m_persistence.removeAttributes();
}

void DataService::publishParameters()
{
    std::vector<Parameter> parameters;
    for (const auto& element : m_persistence.getParameters())
    {
        parameters.emplace_back(element.first, element.second);
    }

    if (parameters.empty())
    {
        return;
    }

    ParametersUpdateMessage parametersUpdateMessage(parameters);

    const std::shared_ptr<Message> outboundMessage =
      m_protocol.makeOutboundMessage(m_deviceKey, parametersUpdateMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from parameters";
        return;
    }

    if (m_connectivityService.publish(outboundMessage))
        for (const auto& parameter : parameters)
            m_persistence.removeParameters(parameter.first);
}

const Protocol& DataService::getProtocol()
{
    return m_protocol;
}

void DataService::messageReceived(std::shared_ptr<Message> message)
{
    assert(message);

    const std::string deviceKey = m_protocol.extractDeviceKeyFromChannel(message->getChannel());
    if (deviceKey.empty())
    {
        LOG(WARN) << "Unable to extract device key from channel: " << message->getChannel();
        return;
    }

    if (deviceKey != m_deviceKey)
    {
        LOG(WARN) << "Device key mismatch: " << message->getChannel();
        return;
    }

    switch (m_protocol.getMessageType(message))
    {
    case MessageType::FEED_VALUES:
    {
        auto feedValuesMessage = m_protocol.parseFeedValues(message);
        if (feedValuesMessage == nullptr)
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        else if (m_feedUpdateHandler)
            m_feedUpdateHandler(feedValuesMessage->getReadings());
        return;
    }
    case MessageType::PARAMETER_SYNC:
    {
        auto parameterMessage = m_protocol.parseParameters(message);
        if (parameterMessage == nullptr)
        {
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
            return;
        }

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
                auto allNamesMatching = std::all_of(parameters.cbegin(), parameters.cend(),
                                                    [&](const ParameterName& name)
                                                    {
                                                        return std::find_if(values.begin(), values.end(),
                                                                            [&](const Parameter& parameter) {
                                                                                return parameter.first == name;
                                                                            }) != values.cend();
                                                    });
                if (!allNamesMatching)
                {
                    continue;
                }

                // Then we found the ones for the subscription!
                auto callback = subscription.second.callback;
                m_commandBuffer.pushCommand(
                  std::make_shared<std::function<void()>>([callback, values]() { callback(values); }));

                // And we can clear the subscription
                m_parameterSubscriptions.erase(subscription.first);
                return;
            }
        }

        if (m_parameterSyncHandler)
            m_parameterSyncHandler(parameterMessage->getParameters());
        return;
    }
    default:
    {
        LOG(WARN) << "Unable to parse message channel: " << message->getChannel();
    }
    }
}

std::string DataService::makePersistenceKey(const std::string& deviceKey, const std::string& reference) const
{
    return deviceKey + PERSISTENCE_KEY_DELIMITER + reference;
}

std::pair<std::string, std::string> DataService::parsePersistenceKey(const std::string& key) const
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

std::vector<std::string> DataService::findMatchingPersistanceKeys(const std::string& deviceKey,
                                                                  const std::vector<std::string>& persistenceKeys) const
{
    std::vector<std::string> matchingKeys;

    for (const auto& key : persistenceKeys)
    {
        auto pair = parsePersistenceKey(key);
        if (pair.first == deviceKey)
        {
            matchingKeys.push_back(key);
        }
    }

    return matchingKeys;
}

void DataService::publishReadingsForPersistenceKey(const std::string& persistenceKey)
{
    auto readings = std::vector<Reading>{};
    for (const auto& readingFromPersistence : m_persistence.getReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT))
        readings.emplace_back(*readingFromPersistence);
    if (readings.empty())
        return;

    FeedValuesMessage feedValuesMessage(readings);

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeOutboundMessage(m_deviceKey, feedValuesMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from readings: " << persistenceKey;
        m_persistence.removeReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);
        return;
    }

    if (m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);

        // proceed to publish next batch only if publish is successful
        publishReadingsForPersistenceKey(persistenceKey);
    }
}
}    // namespace wolkabout

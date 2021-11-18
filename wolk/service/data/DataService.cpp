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

#include "core/connectivity/ConnectivityService.h"
#include "core/model/Message.h"
#include "core/model/Feed.h"
#include "core/model/Attribute.h"
#include "core/Types.h"
#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/utilities/Logger.h"
#include "wolk/service/data/DataService.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace wolkabout
{
DataService::DataService(std::string deviceKey, DataProtocol& protocol, Persistence& persistence,
                         ConnectivityService& connectivityService, FeedUpdateSetHandler feedUpdateHandler,
                         ParameterSyncHandler parameterSyncHandler)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_persistence{persistence}
, m_connectivityService{connectivityService}
, m_feedUpdateHandler{std::move(feedUpdateHandler)}
, m_parameterSyncHandler{std::move(parameterSyncHandler)}
{
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
        if (!feedValuesMessage)
        {
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        }
        if (m_feedUpdateHandler)
        {
            m_feedUpdateHandler(feedValuesMessage->getReadings());
        }
    }
    case MessageType::PARAMETER_SYNC:
    {
        auto parameterMessage = m_protocol.parseParameters(message);
        if (!parameterMessage)
        {
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        }
        if (m_parameterSyncHandler)
        {
            m_parameterSyncHandler(parameterMessage->getParameters());
        }
    }
    default:
    {
        LOG(WARN) << "Unable to parse message channel: " << message->getChannel();
    }
    }
}

const Protocol& DataService::getProtocol()
{
    return m_protocol;
}

void DataService::addReading(const std::string& reference, const std::string& value, std::uint64_t rtc)
{
    auto sensorReading = std::make_shared<Reading>(reference, value, rtc);

    m_persistence.putReading(reference, sensorReading);
}

void DataService::publishReadings()
{
    for (const auto& key : m_persistence.getReadingsKeys())
    {
        publishReadingsForPersistenceKey(key);
    }
}

void DataService::publishAttributes()
{
    const auto attributes = m_persistence.getAttributes();

    if (attributes.empty())
    {
        return;
    }

    AttributeRegistrationMessage attributeRegistrationMessage(attributes);

    const std::shared_ptr<Message> outboundMessage =
      m_protocol.makeOutboundMessage(m_deviceKey, attributeRegistrationMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from attributes";
        m_persistence.removeAttributes();
        return;
    }

    if (m_connectivityService.publish(outboundMessage, false))
    {
        m_persistence.removeAttributes();
    }
}

void DataService::publishParameters()
{
    std::vector<Parameters> parameters;
    for (auto element : m_persistence.getParameters())
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
        m_persistence.removeAttributes();
        return;
    }

    if (m_connectivityService.publish(outboundMessage, false))
    {
        m_persistence.removeAttributes();
    }
}

void DataService::publishReadingsForPersistenceKey(const std::string& persistenceKey)
{
    const auto readings = m_persistence.getReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);

    if (readings.empty())
    {
        return;
    }

    FeedValuesMessage feedValuesMessage(readings);

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeOutboundMessage(m_deviceKey, feedValuesMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from readings: " << persistenceKey;
        m_persistence.removeReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);
        return;
    }

    if (m_connectivityService.publish(outboundMessage, false))
    {
        m_persistence.removeReadings(persistenceKey, PUBLISH_BATCH_ITEMS_COUNT);

        // proceed to publish next batch only if publish is successful
        publishReadingsForPersistenceKey(persistenceKey);
    }
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

    if (!m_connectivityService.publish(outboundMessage, false))
    {
        LOG(ERROR) << "Unable to publish feed registration message";
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

    if (!m_connectivityService.publish(outboundMessage, false))
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

    if (!m_connectivityService.publish(outboundMessage, false))
    {
        LOG(ERROR) << "Unable to publish Pull Parameters message";
    }
}
void DataService::addAttribute(Attribute attribute)
{
    auto attr = std::make_shared<Attribute>(attribute);

    m_persistence.putAttribute(attr);
}
void DataService::updateParameter(Parameters parameter)
{
    m_persistence.putParameter(parameter);
}

}    // namespace wolkabout

/*
 * Copyright 2018 WolkAbout Technology s.r.o.
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

#include "DataService.h"

#include "core/connectivity/ConnectivityService.h"
#include "core/model/Message.h"
#include "core/model/Feed.h"
#include "core/model/Attribute.h"
#include "core/Types.h"
#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/utilities/Logger.h"

#include <algorithm>
#include <cassert>

namespace wolkabout
{
DataService::DataService(std::string deviceKey, DataProtocol& protocol, Persistence& persistence,
                         ConnectivityService& connectivityService, const FeedUpdateSetHandler& feedUpdateHandler)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_persistence{persistence}
, m_connectivityService{connectivityService}
, m_feedUpdateHandler{feedUpdateHandler}
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
        if(!feedValuesMessage)
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
        if(!parameterMessage)
        {
            LOG(WARN) << "Unable to parse message: " << message->getChannel();
        }
        // TODO implement the commented part of the code
//        if(m_configurationSetHandler)
//        {
//            m_configurationSetHandler(parameterMessage->getParameters());
//        }

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

void DataService::addReading(const std::string& reference, const std::string& value, unsigned long long int rtc)
{
    auto sensorReading = std::make_shared<Reading>(value, reference, rtc);

    m_persistence.putReading(reference, sensorReading);
}

void DataService::publishReadings()
{
    for (const auto& key : m_persistence.getReadingsKeys())
    {
        publishReadingsForPersistenceKey(key);
    }
}

void DataService::publishReadingsForPersistenceKey(const std::string& persistanceKey)
{
    const auto readings = m_persistence.getReadings(persistanceKey, PUBLISH_BATCH_ITEMS_COUNT);

    if (readings.empty())
    {
        return;
    }

    FeedValuesMessage feedValuesMessage(readings);

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeOutboundMessage(m_deviceKey, feedValuesMessage);

    if (!outboundMessage)
    {
        LOG(ERROR) << "Unable to create message from readings: " << persistanceKey;
        m_persistence.removeReadings(persistanceKey, PUBLISH_BATCH_ITEMS_COUNT);
        return;
    }

    if (m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeReadings(persistanceKey, PUBLISH_BATCH_ITEMS_COUNT);

        // proceed to publish next batch only if publish is successful
        publishReadingsForPersistenceKey(persistanceKey);
    }
}

}    // namespace wolkabout

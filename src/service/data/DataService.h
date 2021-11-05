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

#ifndef DATASERVICE_H
#define DATASERVICE_H

#include "core/InboundMessageHandler.h"
#include "core/Types.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <core/model/Attribute.h>
#include "core/model/Feed.h"

namespace wolkabout
{
class DataProtocol;
class Persistence;
class ConnectivityService;

typedef std::function<void(std::map<unsigned long long int, std::vector<Reading>>)> FeedUpdateSetHandler;

class DataService : public MessageListener
{
public:
    DataService(std::string deviceKey, DataProtocol& protocol, Persistence& persistence,
                ConnectivityService& connectivityService, const FeedUpdateSetHandler& feedUpdateHandler);

    void messageReceived(std::shared_ptr<Message> message) override;
    const Protocol& getProtocol() override;

    virtual void addReading(const std::string& reference, const std::string& value, unsigned long long int rtc);

    virtual void addAttribute(Attribute attribute);
    virtual void updateParameter(Parameters parameter);

    virtual void registerFeed(Feed feed);
    virtual void registerFeeds(std::vector<Feed> feed);

    virtual void pullFeedValues();
    virtual void pullParameters();

    virtual void publishReadings();

    virtual void publishAttributes();

    virtual void publishParameters();

private:
    std::string getValueDelimiter(const std::string& key) const;

    void publishReadingsForPersistenceKey(const std::string& persistenceKey);
    void publishAttributesForPersistenceKey(const std::string& persistenceKey);
    void publishParametersForPersistenceKey(const std::string& persistenceKey);

    const std::string m_deviceKey;

    DataProtocol& m_protocol;
    Persistence& m_persistence;
    ConnectivityService& m_connectivityService;

    FeedUpdateSetHandler m_feedUpdateHandler;

    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;
};
}    // namespace wolkabout

#endif

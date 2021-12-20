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

#ifndef DATASERVICE_H
#define DATASERVICE_H

#include "core/InboundMessageHandler.h"
#include "core/Types.h"
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/model/Reading.h"
#include "core/utilities/CommandBuffer.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class DataProtocol;
class Persistence;
class ConnectivityService;

using FeedUpdateSetHandler = std::function<void(std::map<std::uint64_t, std::vector<Reading>>)>;
using ParameterSyncHandler = std::function<void(std::vector<Parameter>)>;

class DataService : public MessageListener
{
public:
    DataService(std::string deviceKey, DataProtocol& protocol, Persistence& persistence,
                ConnectivityService& connectivityService, FeedUpdateSetHandler feedUpdateHandler,
                ParameterSyncHandler parameterSyncHandler);

    virtual void addReading(const std::string& reference, const std::string& value, std::uint64_t rtc);
    virtual void addReading(const std::string& reference, const std::vector<std::string>& value, std::uint64_t rtc);

    virtual void addAttribute(const Attribute& attribute);
    virtual void updateParameter(Parameter parameter);

    virtual void registerFeed(Feed feed);
    virtual void registerFeeds(std::vector<Feed> feed);

    virtual void removeFeed(std::string reference);
    virtual void removeFeeds(std::vector<std::string> feeds);

    virtual void pullFeedValues();
    virtual void pullParameters();
    virtual bool synchronizeParameters(const std::vector<ParameterName>& parameters,
                                       std::function<void(std::vector<Parameter>)> callback);

    virtual void publishReadings();
    virtual void publishReadings(const std::string& deviceKey);

    virtual void publishAttributes();

    virtual void publishParameters();

    const Protocol& getProtocol() override;

    void messageReceived(std::shared_ptr<Message> message) override;

private:
    std::string makePersistenceKey(const std::string& deviceKey, const std::string& reference) const;

    std::pair<std::string, std::string> parsePersistenceKey(const std::string& key) const;

    std::vector<std::string> findMatchingPersistanceKeys(const std::string& deviceKey,
                                                         const std::vector<std::string>& persistenceKeys) const;

    void publishReadingsForPersistenceKey(const std::string& persistenceKey);

    const std::string m_deviceKey;

    DataProtocol& m_protocol;
    Persistence& m_persistence;
    ConnectivityService& m_connectivityService;

    FeedUpdateSetHandler m_feedUpdateHandler;
    ParameterSyncHandler m_parameterSyncHandler;

    CommandBuffer m_commandBuffer;
    struct ParameterSubscription
    {
        std::vector<ParameterName> parameters;
        std::function<void(std::vector<Parameter>)> callback;
    };
    std::uint64_t m_iterator;
    std::mutex m_subscriptionMutex;
    std::map<std::uint64_t, ParameterSubscription> m_parameterSubscriptions;

    static const std::string PERSISTENCE_KEY_DELIMITER;
    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;
};
}    // namespace wolkabout

#endif

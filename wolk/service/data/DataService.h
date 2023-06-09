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

#ifndef DATASERVICE_H
#define DATASERVICE_H

#include "core/Types.h"
#include "core/connectivity/InboundMessageHandler.h"
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
// Forward declare some interfaces from the SDK
class DataProtocol;
class DetailsSynchronizationResponseMessage;
class ParametersUpdateMessage;
class Persistence;
class ConnectivityService;
class OutboundRetryMessageHandler;

namespace connect
{
using FeedUpdateSetHandler = std::function<void(std::string, std::map<std::uint64_t, std::vector<Reading>>)>;
using ParameterSyncHandler = std::function<void(std::string, std::vector<Parameter>)>;
using DetailsSyncHandler = std::function<void(std::string, std::vector<std::string>, std::vector<std::string>)>;

class DataService : public MessageListener
{
public:
    DataService(DataProtocol& protocol, Persistence& persistence, ConnectivityService& connectivityService,
                OutboundRetryMessageHandler& outboundRetryMessageHandler, FeedUpdateSetHandler feedUpdateHandler,
                ParameterSyncHandler parameterSyncHandler, DetailsSyncHandler detailsSyncHandler);

    virtual void addReading(const std::string& deviceKey, const std::string& reference, const std::string& value,
                            std::uint64_t rtc);
    virtual void addReading(const std::string& deviceKey, const std::string& reference,
                            const std::vector<std::string>& value, std::uint64_t rtc);

    virtual void addReading(const std::string& deviceKey, const Reading& reading);
    virtual void addReadings(const std::string& deviceKey, const std::vector<Reading>& readings);

    virtual void addAttribute(const std::string& deviceKey, const Attribute& attribute);
    virtual void updateParameter(const std::string& deviceKey, const Parameter& parameter);

    virtual void registerFeed(const std::string& deviceKey, Feed feed);
    virtual void registerFeeds(const std::string& deviceKey, std::vector<Feed> feed);

    virtual void removeFeed(const std::string& deviceKey, std::string reference);
    virtual void removeFeeds(const std::string& deviceKey, std::vector<std::string> feeds);

    virtual void pullFeedValues(const std::string& deviceKey);
    virtual void pullParameters(const std::string& deviceKey);
    virtual bool synchronizeParameters(const std::string& deviceKey, const std::vector<ParameterName>& parameters,
                                       std::function<void(std::vector<Parameter>)> callback);

    virtual bool detailsSynchronizationAsync(
      const std::string& deviceKey, std::function<void(std::vector<std::string>, std::vector<std::string>)> callback);

    virtual void publishReadings();
    virtual void publishReadings(const std::string& deviceKey);

    virtual void publishAttributes();
    virtual void publishAttributes(const std::string& deviceKey);

    virtual void publishParameters();
    virtual void publishParameters(const std::string& deviceKey);

    const Protocol& getProtocol() override;

    void messageReceived(std::shared_ptr<Message> message) override;

private:
    static std::string makePersistenceKey(const std::string& deviceKey, const std::string& reference);

    static std::pair<std::string, std::string> parsePersistenceKey(const std::string& key);

    bool checkIfSubscriptionIsWaiting(const ParametersUpdateMessage& parameterMessage);

    bool checkIfCallbackIsWaiting(const DetailsSynchronizationResponseMessage& synchronizationResponseMessage);

    void publishReadingsForPersistenceKey(const std::string& persistenceKey);

    DataProtocol& m_protocol;
    Persistence& m_persistence;
    ConnectivityService& m_connectivityService;
    OutboundRetryMessageHandler& m_outboundRetryMessageHandler;

    FeedUpdateSetHandler m_feedUpdateHandler;
    ParameterSyncHandler m_parameterSyncHandler;
    DetailsSyncHandler m_detailsSyncHandler;

    legacy::CommandBuffer m_commandBuffer;
    struct ParameterSubscription
    {
        std::vector<ParameterName> parameters;
        std::function<void(std::vector<Parameter>)> callback;
    };
    std::uint64_t m_iterator;
    std::mutex m_subscriptionMutex;
    std::map<std::uint64_t, ParameterSubscription> m_parameterSubscriptions;

    std::mutex m_detailsMutex;
    std::queue<std::function<void(std::vector<std::string>, std::vector<std::string>)>> m_detailsCallbacks;

    static const std::string PERSISTENCE_KEY_DELIMITER;
    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;
};
}    // namespace connect
}    // namespace wolkabout

#endif

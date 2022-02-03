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

#ifndef WOLKABOUTCONNECTOR_DATASERVICEMOCK_H
#define WOLKABOUTCONNECTOR_DATASERVICEMOCK_H

#include "wolk/service/data/DataService.h"

#include <gmock/gmock.h>

using namespace wolkabout;
using namespace wolkabout::connect;

class DataServiceMock : public DataService
{
public:
    DataServiceMock(DataProtocol& protocol, Persistence& persistence, ConnectivityService& connectivityService,
                    OutboundRetryMessageHandler& outboundRetryMessageHandler,
                    const FeedUpdateSetHandler& feedUpdateHandler, const ParameterSyncHandler& parameterSyncHandler,
                    const DetailsSyncHandler& detailsSyncHandler)
    : DataService(protocol, persistence, connectivityService, outboundRetryMessageHandler, feedUpdateHandler,
                  parameterSyncHandler, detailsSyncHandler)
    {
    }
    MOCK_METHOD(void, addReading, (const std::string&, const std::string&, const std::string&, std::uint64_t));
    MOCK_METHOD(void, addReading,
                (const std::string&, const std::string&, const std::vector<std::string>&, std::uint64_t));
    MOCK_METHOD(void, addReading, (const std::string&, const Reading&));
    MOCK_METHOD(void, addReadings, (const std::string&, const std::vector<Reading>&));
    MOCK_METHOD(void, addAttribute, (const std::string&, const Attribute&));
    MOCK_METHOD(void, updateParameter, (const std::string&, const Parameter&));
    MOCK_METHOD(void, registerFeed, (const std::string&, Feed));
    MOCK_METHOD(void, registerFeeds, (const std::string&, std::vector<Feed>));
    MOCK_METHOD(void, removeFeed, (const std::string&, std::string));
    MOCK_METHOD(void, removeFeeds, (const std::string&, std::vector<std::string>));
    MOCK_METHOD(void, pullFeedValues, (const std::string&));
    MOCK_METHOD(void, pullParameters, (const std::string&));
    MOCK_METHOD(bool, synchronizeParameters,
                (const std::string&, const std::vector<ParameterName>&, std::function<void(std::vector<Parameter>)>));
    MOCK_METHOD(bool, detailsSynchronization,
                (const std::string&, std::function<void(std::vector<std::string>, std::vector<std::string>)>));
    MOCK_METHOD(void, publishReadings, ());
    MOCK_METHOD(void, publishReadings, (const std::string&));
    MOCK_METHOD(void, publishAttributes, ());
    MOCK_METHOD(void, publishAttributes, (const std::string&));
    MOCK_METHOD(void, publishParameters, ());
    MOCK_METHOD(void, publishParameters, (const std::string&));
};

#endif    // WOLKABOUTCONNECTOR_DATASERVICEMOCK_H

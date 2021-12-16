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

class DataServiceMock : public DataService
{
public:
    DataServiceMock(const std::string& deviceKey, DataProtocol& protocol, Persistence& persistence,
                    ConnectivityService& connectivityService, const FeedUpdateSetHandler& feedUpdateHandler,
                    const ParameterSyncHandler& parameterSyncHandler)
    : DataService(deviceKey, protocol, persistence, connectivityService, feedUpdateHandler, parameterSyncHandler)
    {
    }
    MOCK_METHOD(void, addReading, (const std::string&, const std::string&, std::uint64_t));
    MOCK_METHOD(void, addReading, (const std::string&, const std::vector<std::string>&, std::uint64_t));
    MOCK_METHOD(void, addAttribute, (const Attribute&));
    MOCK_METHOD(void, updateParameter, (Parameter));
    MOCK_METHOD(void, registerFeed, (Feed));
    MOCK_METHOD(void, registerFeeds, (std::vector<Feed>));
    MOCK_METHOD(void, removeFeed, (std::string));
    MOCK_METHOD(void, removeFeeds, (std::vector<std::string>));
    MOCK_METHOD(void, pullFeedValues, ());
    MOCK_METHOD(void, pullParameters, ());
    MOCK_METHOD(bool, synchronizeParameters,
                (const std::vector<ParameterName>&, std::function<void(std::vector<Parameter>)>));
    MOCK_METHOD(void, publishReadings, ());
    MOCK_METHOD(void, publishAttributes, ());
    MOCK_METHOD(void, publishParameters, ());
};

#endif    // WOLKABOUTCONNECTOR_DATASERVICEMOCK_H

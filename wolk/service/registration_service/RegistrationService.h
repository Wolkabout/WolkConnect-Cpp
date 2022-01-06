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

#ifndef WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H
#define WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H

#include "core/InboundMessageHandler.h"
#include "core/Types.h"
#include "core/model/Attribute.h"
#include "core/model/Feed.h"
#include "core/protocol/RegistrationProtocol.h"
#include "wolk/service/error/ErrorService.h"

namespace wolkabout
{
// Making some type aliases that will make method signatures more readable
struct DeviceInformation
{
    explicit DeviceInformation(std::string name, std::string key = "", std::string guid = "")
    : m_name(std::move(name)), m_key(std::move(key)), m_guid(std::move(guid))
    {
    }

    std::string m_name;
    std::string m_key;
    std::string m_guid;
};
using Feeds = std::vector<Feed>;
using Parameters = std::map<ParameterName, std::string>;
using Attributes = std::vector<Attribute>;

class RegistrationService : public MessageListener
{
public:
    explicit RegistrationService(RegistrationProtocol& protocol, ErrorService& errorService);

    bool registerDevice(DeviceInformation information, Feeds feeds, Parameters parameters, Attributes attributes);

    bool removeDevices(std::vector<std::string> deviceKeys);

    std::unique_ptr<std::vector<DeviceInformation>> obtainDevices(TimePoint timestampFrom, std::string deviceType = {},
                                                                  std::string externalId = {});

    void messageReceived(std::shared_ptr<Message> message) override;

    const Protocol& getProtocol() override;

private:
    // Here we store the actual protocol.
    RegistrationProtocol& m_protocol;

    // And here we have the reference of the error service.
    ErrorService& m_errorService;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_REGISTRATIONSERVICE_H

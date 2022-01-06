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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied .
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wolk/service/registration_service/RegistrationService.h"

#include "core/utilities/Logger.h"

namespace wolkabout
{
RegistrationService::RegistrationService(RegistrationProtocol& protocol, ErrorService& errorService)
: m_protocol(protocol), m_errorService(errorService)
{
}

bool RegistrationService::registerDevice(DeviceInformation information, Feeds feeds, Parameters parameters,
                                         Attributes attributes)
{
    LOG(TRACE) << METHOD_INFO;

    return false;
}

bool RegistrationService::removeDevices(std::vector<std::string> deviceKeys)
{
    LOG(TRACE) << METHOD_INFO;

    return false;
}

std::unique_ptr<std::vector<DeviceInformation>> RegistrationService::obtainDevices(TimePoint timestampFrom,
                                                                                   std::string deviceType,
                                                                                   std::string externalId)
{
    LOG(TRACE) << METHOD_INFO;

    return {};
}

void RegistrationService::messageReceived(std::shared_ptr<Message> message) {}

const Protocol& RegistrationService::getProtocol()
{
    return m_protocol;
}
}    // namespace wolkabout

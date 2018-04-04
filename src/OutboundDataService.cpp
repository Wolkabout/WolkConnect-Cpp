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

#include "OutboundDataService.h"
#include "connectivity/ConnectivityService.h"
#include "connectivity/json/JsonSingleOutboundMessageFactory.h"
#include "model/FirmwareUpdateResponse.h"

#include <iostream>

namespace wolkabout
{
OutboundDataService::OutboundDataService(Device device, OutboundMessageFactory& outboundMessageFactory,
                                         ConnectivityService& connectivityService)
: m_device{device}, m_outboundMessageFactory(outboundMessageFactory), m_connectivityService(connectivityService)
{
}

void OutboundDataService::addFirmwareUpdateResponse(const FirmwareUpdateResponse& firmwareUpdateResponse)
{
    const std::shared_ptr<OutboundMessage> outboundMessage =
      m_outboundMessageFactory.makeFromFirmwareUpdateResponse(firmwareUpdateResponse);

    if (outboundMessage && m_connectivityService.publish(outboundMessage))
    {
        // TODO: Log
    }
}

void OutboundDataService::addFilePacketRequest(const FilePacketRequest& filePacketRequest)
{
    const std::shared_ptr<OutboundMessage> outboundMessage =
      m_outboundMessageFactory.makeFromFilePacketRequest(filePacketRequest);

    if (outboundMessage && m_connectivityService.publish(outboundMessage))
    {
        // TODO: Log
    }
}

}    // namespace wolkabout

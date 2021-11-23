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

#ifndef WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICE_H
#define WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICE_H

#include "core/connectivity/ConnectivityService.h"
#include "core/InboundMessageHandler.h"
#include "core/protocol/FirmwareUpdateProtocol.h"

namespace wolkabout
{
class FirmwareUpdateService : public MessageListener
{
public:
    FirmwareUpdateService(ConnectivityService& connectivityService, FirmwareUpdateProtocol& protocol);

    void messageReceived(std::shared_ptr<Message> message) override;

    const Protocol& getProtocol() override;

private:
    void onFirmwareInstall(const std::string& deviceKey, const FirmwareUpdateInstallMessage& message);

    void onFirmwareAbort(const std::string& deviceKey, const FirmwareUpdateAbortMessage& message);

    // This is where we store the message sender
    ConnectivityService& m_connectivityService;

    // This is where the protocol will be passed while the service is created.
    FirmwareUpdateProtocol& m_protocol;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICE_H

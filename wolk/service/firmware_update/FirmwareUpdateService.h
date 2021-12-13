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

#include "core/InboundMessageHandler.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/protocol/FirmwareUpdateProtocol.h"
#include "wolk/api/FirmwareInstaller.h"
#include "wolk/api/FirmwareParametersListener.h"
#include "wolk/service/data/DataService.h"

#include <queue>

namespace wolkabout
{
class FirmwareUpdateService : public MessageListener
{
public:
    FirmwareUpdateService(std::string deviceKey, ConnectivityService& connectivityService, DataService& dataService,
                          std::shared_ptr<FirmwareInstaller> firmwareInstaller, FirmwareUpdateProtocol& protocol,
                          const std::string& workingDirectory = "./");

    FirmwareUpdateService(std::string deviceKey, ConnectivityService& connectivityService, DataService& dataService,
                          std::shared_ptr<FirmwareParametersListener> firmwareParametersListener,
                          FirmwareUpdateProtocol& protocol, const std::string& workingDirectory = "./");

    void setup();

    void connected();

    const Protocol& getProtocol() override;

    void messageReceived(std::shared_ptr<Message> message) override;

private:
    void onFirmwareInstall(const std::string& deviceKey, const FirmwareUpdateInstallMessage& message);

    void onFirmwareAbort(const std::string& deviceKey, const FirmwareUpdateAbortMessage& message);

    void sendStatusMessage(FirmwareUpdateStatus status, FirmwareUpdateError error = FirmwareUpdateError::NONE);

    void queueStatusMessage(FirmwareUpdateStatus status, FirmwareUpdateError error = FirmwareUpdateError::NONE);

    bool storeSessionFile();

    void deleteSessionFile();

    void reportParameters();

    void obtainParametersAndAnnounce();

    // This is where we store the message sender
    ConnectivityService& m_connectivityService;
    DataService& m_dataService;
    const std::string m_deviceKey;
    std::string m_sessionFile;

    // Here we store the info if a session is ongoing
    std::atomic_bool m_installation;

    // Here we store messages that the service queues up to send when the connection is established
    std::queue<std::shared_ptr<Message>> m_queue;

    // There is one of two ways the firmware update service can be instantiated
    std::shared_ptr<FirmwareInstaller> m_firmwareInstaller;
    std::shared_ptr<FirmwareParametersListener> m_firmwareParametersListener;

    // This is where the protocol will be passed while the service is created.
    FirmwareUpdateProtocol& m_protocol;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICE_H

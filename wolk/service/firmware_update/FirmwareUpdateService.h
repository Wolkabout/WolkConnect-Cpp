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
#include "core/connectivity/InboundMessageHandler.h"
#include "core/protocol/FirmwareUpdateProtocol.h"
#include "wolk/api/FirmwareInstaller.h"
#include "wolk/api/FirmwareParametersListener.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/file_management/FileManagementService.h"

#include <queue>

namespace wolkabout
{
namespace connect
{
class FirmwareUpdateService : public MessageListener
{
public:
    FirmwareUpdateService(ConnectivityService& connectivityService, DataService& dataService,
                          std::shared_ptr<FileManagementService> fileManagementService,
                          std::unique_ptr<FirmwareInstaller> firmwareInstaller, FirmwareUpdateProtocol& protocol,
                          const std::string& workingDirectory = "./");

    FirmwareUpdateService(ConnectivityService& connectivityService, DataService& dataService,
                          std::shared_ptr<FileManagementService> fileManagementService,
                          std::unique_ptr<FirmwareParametersListener> firmwareParametersListener,
                          FirmwareUpdateProtocol& protocol, const std::string& workingDirectory = "./");

    bool isInstaller() const;

    bool isParameterListener() const;

    virtual std::string getVersionForDevice(const std::string& deviceKey);

    /**
     * This is the queue containing any messages the service might want to send.
     *
     * @return The reference to the queue of messages.
     */
    std::queue<std::shared_ptr<Message>>& getQueue();

    /**
     * This is a loadState method that should be invoked to understand what the state of firmware update is.
     *
     * @param deviceKey The device key for which the state should be loaded.
     */
    virtual void loadState(const std::string& deviceKey);

    /**
     * This is a method that will
     */
    virtual void obtainParametersAndAnnounce(const std::string& deviceKey);

    const Protocol& getProtocol() override;

    void messageReceived(std::shared_ptr<Message> message) override;

private:
    void onFirmwareInstall(const std::string& deviceKey, const FirmwareUpdateInstallMessage& message);

    void onFirmwareAbort(const std::string& deviceKey, const FirmwareUpdateAbortMessage& message);

    void sendStatusMessage(const std::string& deviceKey, FirmwareUpdateStatus status,
                           FirmwareUpdateError error = FirmwareUpdateError::NONE);

    void queueStatusMessage(const std::string& deviceKey, FirmwareUpdateStatus status,
                            FirmwareUpdateError error = FirmwareUpdateError::NONE);

    bool storeSessionFile(const std::string& deviceKey, const std::string& version);

    void deleteSessionFile(const std::string& deviceKey);

    // This is where we store the message sender
    ConnectivityService& m_connectivityService;
    DataService& m_dataService;
    std::shared_ptr<FileManagementService> m_fileManagementService;
    std::string m_sessionFile;

    // Here we store the info if a session is ongoing
    std::map<std::string, std::atomic_bool> m_installation;

    // Here we store messages that the service queues up to send when the connection is established
    std::queue<std::shared_ptr<Message>> m_queue;

    // There is one of two ways the firmware update service can be instantiated
    std::unique_ptr<FirmwareInstaller> m_firmwareInstaller;
    std::unique_ptr<FirmwareParametersListener> m_firmwareParametersListener;

    // This is where the protocol will be passed while the service is created.
    FirmwareUpdateProtocol& m_protocol;

    // The command buffer where the installations will be executed.
    CommandBuffer m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FIRMWAREUPDATESERVICE_H

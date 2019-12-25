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
#ifndef FIRMWAREUPDATESERVICE_H
#define FIRMWAREUPDATESERVICE_H

#include "InboundMessageHandler.h"
#include "repository/FileRepository.h"
#include "utilities/CommandBuffer.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace wolkabout
{
class ConnectivityService;
class FirmwareInstaller;
class FirmwareVersionProvider;
class FirmwareUpdateAbort;
class FirmwareUpdateInstall;
class FirmwareUpdateResponse;
class FirmwareUpdateStatus;
class JsonDFUProtocol;

class FirmwareUpdateService : public MessageListener
{
public:
    FirmwareUpdateService(std::string deviceKey, JsonDFUProtocol& protocol, FileRepository& fileRepository,
                          std::shared_ptr<FirmwareInstaller> firmwareInstaller,
                          std::shared_ptr<FirmwareVersionProvider> firmwareVersionProvider,
                          ConnectivityService& connectivityService);

    void messageReceived(std::shared_ptr<Message> message) override;
    const Protocol& getProtocol() override;

    void publishFirmwareVersion();

    void reportFirmwareUpdateResult();

private:
    void handleFirmwareUpdateCommand(const FirmwareUpdateInstall& command);
    void handleFirmwareUpdateCommand(const FirmwareUpdateAbort& command);

    void install(const std::string& firmwareFileName);

    void installSucceeded();

    void installFailed();

    void abort();

    void sendStatus(const FirmwareUpdateStatus& status);

    void addToCommandBuffer(std::function<void()> command);

    const std::string m_deviceKey;

    JsonDFUProtocol& m_protocol;

    FileRepository& m_fileRepository;

    std::shared_ptr<FirmwareInstaller> m_firmwareInstaller;
    std::shared_ptr<FirmwareVersionProvider> m_firmwareVersionProvider;

    ConnectivityService& m_connectivityService;

    CommandBuffer m_commandBuffer;

    static const constexpr char* FIRMWARE_VERSION_FILE = ".dfu-version";
};
}    // namespace wolkabout

#endif    // FIRMWAREUPDATESERVICE_H
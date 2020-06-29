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

#include "service/FirmwareUpdateService.h"
#include "FirmwareInstaller.h"
#include "FirmwareVersionProvider.h"
#include "connectivity/ConnectivityService.h"
#include "model/FirmwareUpdateAbort.h"
#include "model/FirmwareUpdateInstall.h"
#include "model/FirmwareUpdateResponse.h"
#include "model/FirmwareUpdateStatus.h"
#include "model/FirmwareVersion.h"
#include "model/Message.h"
#include "protocol/json/JsonDFUProtocol.h"
#include "utilities/FileSystemUtils.h"
#include "utilities/Logger.h"
#include "utilities/StringUtils.h"

namespace wolkabout
{
FirmwareUpdateService::FirmwareUpdateService(std::string deviceKey, JsonDFUProtocol& protocol,
                                             FileRepository& fileRepository,
                                             std::shared_ptr<FirmwareInstaller> firmwareInstaller,
                                             std::shared_ptr<FirmwareVersionProvider> firmwareVersionProvider,
                                             ConnectivityService& connectivityService)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_fileRepository{fileRepository}
, m_firmwareInstaller{firmwareInstaller}
, m_firmwareVersionProvider{firmwareVersionProvider}
, m_connectivityService{connectivityService}
{
}

void FirmwareUpdateService::messageReceived(std::shared_ptr<Message> message)
{
    auto installCommand = m_protocol.makeFirmwareUpdateInstall(*message);
    if (installCommand)
    {
        auto installDto = *installCommand;
        addToCommandBuffer([=] { handleFirmwareUpdateCommand(installDto); });

        return;
    }

    auto abortCommand = m_protocol.makeFirmwareUpdateAbort(*message);
    if (abortCommand)
    {
        auto abortDto = *abortCommand;
        addToCommandBuffer([=] { handleFirmwareUpdateCommand(abortDto); });

        return;
    }

    LOG(WARN) << "Unable to parse message; channel: " << message->getChannel()
              << ", content: " << message->getContent();
}

const Protocol& FirmwareUpdateService::getProtocol()
{
    return m_protocol;
}

void FirmwareUpdateService::reportFirmwareUpdateResult()
{
    if (!FileSystemUtils::isFilePresent(FIRMWARE_VERSION_FILE))
    {
        return;
    }

    std::string storedFirmwareVersion;
    FileSystemUtils::readFileContent(FIRMWARE_VERSION_FILE, storedFirmwareVersion);

    StringUtils::removeTrailingWhitespace(storedFirmwareVersion);

    const std::string currentFirmwareVersion = m_firmwareVersionProvider->getFirmwareVersion();

    if (currentFirmwareVersion.empty())
    {
        LOG(WARN) << "Failed to get device's firmware version";
        return;
    }

    if (currentFirmwareVersion != storedFirmwareVersion)
    {
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Status::COMPLETED});
    }
    else
    {
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::INSTALLATION_FAILED});
    }

    FileSystemUtils::deleteFile(FIRMWARE_VERSION_FILE);
}

void FirmwareUpdateService::publishFirmwareVersion()
{
    addToCommandBuffer([=] {
        const std::string firmwareVersion = m_firmwareVersionProvider->getFirmwareVersion();

        if (firmwareVersion.empty())
        {
            LOG(WARN) << "Failed to get device's firmware version";
            return;
        }
      
        const std::shared_ptr<Message> message =
          m_protocol.makeMessage(m_deviceKey, FirmwareVersion{m_deviceKey, firmwareVersion});

        if (!message)
        {
            LOG(WARN) << "Failed to create firmware version message";
            return;
        }

        if (!m_connectivityService.publish(message))
        {
            LOG(WARN) << "Failed to publish firmware version message";
            return;
        }
    });
}

void FirmwareUpdateService::handleFirmwareUpdateCommand(const FirmwareUpdateInstall& command)
{
    if (command.getDeviceKeys().size() != 1 || command.getDeviceKeys().at(0).empty())
    {
        LOG(WARN) << "Unable to extract device key from firmware install command";
        return;
    }

    auto deviceKey = command.getDeviceKeys().at(0);

    if (deviceKey != m_deviceKey)
    {
        LOG(WARN) << "Firmware update command contains unknown device key: " << deviceKey;
        return;
    }

    auto firmwareFile = command.getFileName();

    if (firmwareFile.empty())
    {
        LOG(WARN) << "Missing file path in firmware install command";

        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::FILE_SYSTEM_ERROR});
        return;
    }

    if (!FileSystemUtils::isFilePresent(firmwareFile))
    {
        LOG(WARN) << "Missing firmware file: " << firmwareFile;

        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::FILE_SYSTEM_ERROR});
        return;
    }

    install(firmwareFile);
}

void FirmwareUpdateService::handleFirmwareUpdateCommand(const FirmwareUpdateAbort& command)
{
    if (command.getDeviceKeys().size() != 1 || command.getDeviceKeys().at(0).empty())
    {
        LOG(WARN) << "Unable to extract device key from firmware abort command";
        return;
    }

    auto deviceKey = command.getDeviceKeys().at(0);

    if (deviceKey != m_deviceKey)
    {
        LOG(WARN) << "Firmware update command contains unknown device key: " << deviceKey;
        return;
    }

    abort();
}

void FirmwareUpdateService::install(const std::string& firmwareFile)
{
    LOG(INFO) << "Handling device firmware install";

    if (!m_firmwareInstaller)
    {
        LOG(ERROR) << "Firmware installer not set for device";
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::UNSPECIFIED_ERROR});
        return;
    }

    if (!m_firmwareVersionProvider)
    {
        LOG(ERROR) << "Firmware version provider not set for device";
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::UNSPECIFIED_ERROR});
        return;
    }

    const std::string firmwareVersion = m_firmwareVersionProvider->getFirmwareVersion();

    if (firmwareVersion.empty())
    {
        LOG(WARN) << "Failed to get device's firmware version";
        return;
    }

    if (!FileSystemUtils::createFileWithContent(FIRMWARE_VERSION_FILE, firmwareVersion))
    {
        LOG(ERROR) << "Failed to create firmware version file";
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::FILE_NOT_PRESENT});
        return;
    }

    auto fileInfo = m_fileRepository.getFileInfo(firmwareFile);
    if (!fileInfo)
    {
        LOG(ERROR) << "Missing file info: " << firmwareFile;
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::FILE_SYSTEM_ERROR});
        return;
    }

    sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Status::INSTALLATION});

    m_firmwareInstaller->install(fileInfo->path, [=]() { installSucceeded(); }, [=]() { installFailed(); });
}

void FirmwareUpdateService::installSucceeded()
{
    sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Status::COMPLETED});
    publishFirmwareVersion();
}

void FirmwareUpdateService::installFailed()
{
    sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Error::INSTALLATION_FAILED});
}

void FirmwareUpdateService::abort()
{
    LOG(INFO) << "Abort device firmware installation";
    if (m_firmwareInstaller->abort())
    {
        LOG(INFO) << "Device firmware installation aborted";
        sendStatus(FirmwareUpdateStatus{{m_deviceKey}, FirmwareUpdateStatus::Status::ABORTED});
    }
    else
    {
        LOG(INFO) << "Device firmware installation cannot be aborted";
    }
}

void FirmwareUpdateService::sendStatus(const FirmwareUpdateStatus& response)
{
    auto& deviceKey = response.getDeviceKeys().at(0);
    std::shared_ptr<Message> message = m_protocol.makeMessage(deviceKey, response);

    if (!message)
    {
        LOG(WARN) << "Failed to create firmware update response";
        return;
    }

    if (!m_connectivityService.publish(message))
    {
        LOG(WARN) << "Firmware update response not published for device: " << deviceKey;
    }
}

void FirmwareUpdateService::addToCommandBuffer(std::function<void()> command)
{
    m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>(command));
}
}    // namespace wolkabout
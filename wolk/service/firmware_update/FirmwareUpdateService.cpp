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

#include "wolk/service/firmware_update/FirmwareUpdateService.h"

#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"

#include <utility>

namespace wolkabout
{
namespace connect
{
const std::string SESSION_FILE = ".fw-session";

FirmwareUpdateService::FirmwareUpdateService(ConnectivityService& connectivityService, DataService& dataService,
                                             std::unique_ptr<FirmwareInstaller> firmwareInstaller,
                                             FirmwareUpdateProtocol& protocol, const std::string& workingDirectory)
: m_connectivityService(connectivityService)
, m_dataService(dataService)
, m_sessionFile(FileSystemUtils::composePath(SESSION_FILE, workingDirectory))
, m_firmwareInstaller(std::move(firmwareInstaller))
, m_protocol(protocol)
{
}

FirmwareUpdateService::FirmwareUpdateService(ConnectivityService& connectivityService, DataService& dataService,
                                             std::unique_ptr<FirmwareParametersListener> firmwareParametersListener,
                                             FirmwareUpdateProtocol& protocol, const std::string& workingDirectory)
: m_connectivityService(connectivityService)
, m_dataService(dataService)
, m_sessionFile(FileSystemUtils::composePath(SESSION_FILE, workingDirectory))
, m_firmwareParametersListener(std::move(firmwareParametersListener))
, m_protocol(protocol)
{
}

bool FirmwareUpdateService::isInstaller() const
{
    return m_firmwareInstaller != nullptr;
}

bool FirmwareUpdateService::isParameterListener() const
{
    return m_firmwareParametersListener != nullptr;
}

std::string FirmwareUpdateService::getVersionForDevice(const std::string& deviceKey)

{
    auto firmwareVersion = std::string{};
    if (m_firmwareInstaller != nullptr)
        firmwareVersion = m_firmwareInstaller->getFirmwareVersion(deviceKey);
    else if (m_firmwareParametersListener != nullptr)
        firmwareVersion = m_firmwareParametersListener->getFirmwareVersion();
    return firmwareVersion;
}

std::queue<std::shared_ptr<Message>>& FirmwareUpdateService::getQueue()
{
    return m_queue;
}

void FirmwareUpdateService::loadState(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if there is a session going on, and that we have a firmware installer
    const auto deviceSessionFile = m_sessionFile + "_" + deviceKey;
    if (FileSystemUtils::isFilePresent(deviceSessionFile))
    {
        // If a file installer is not present, this means the user had removed a firmware installer, even if we have a
        // session going on.
        if (m_firmwareInstaller == nullptr)
        {
            LOG(WARN) << "Detected a Firmware Update session but a firmware installer is missing now.";
            deleteSessionFile(deviceKey);
            queueStatusMessage(deviceKey, FirmwareUpdateStatus::ERROR, FirmwareUpdateError::UNKNOWN);
            return;
        }

        // Read the old version of the firmware
        auto content = std::string{};
        if (!FileSystemUtils::readFileContent(deviceSessionFile, content))
        {
            LOG(WARN) << "Failed to read the content of the session file.";
            deleteSessionFile(deviceKey);
            queueStatusMessage(deviceKey, FirmwareUpdateStatus::ERROR, FirmwareUpdateError::UNKNOWN);
            return;
        }

        // Ask the installer if the update went well
        auto success = m_firmwareInstaller->wasFirmwareInstallSuccessful(deviceKey, content);
        if (success)
            queueStatusMessage(deviceKey, FirmwareUpdateStatus::SUCCESS);
        else
            queueStatusMessage(deviceKey, FirmwareUpdateStatus::ERROR, FirmwareUpdateError::INSTALLATION_FAILED);
        deleteSessionFile(deviceKey);
    }
}

void FirmwareUpdateService::obtainParametersAndAnnounce(const std::string& deviceKey)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if there is a parameter listener
    if (m_firmwareParametersListener == nullptr)
    {
        LOG(WARN) << "Cannot obtain parameters - Missing the parameter listener.";
        return;
    }

    // Subscribe to the parameters
    auto parameters =
      std::vector<ParameterName>{ParameterName::FIRMWARE_UPDATE_REPOSITORY, ParameterName::FIRMWARE_UPDATE_CHECK_TIME};
    auto callback = [this](const std::vector<Parameter>& receivedParameters) {
        // Analyze the parameters
        auto repository = std::string{};
        auto checkTime = std::string{};
        for (const auto& parameter : receivedParameters)
            if (parameter.first == ParameterName::FIRMWARE_UPDATE_REPOSITORY)
                repository = parameter.second;
            else if (parameter.first == ParameterName::FIRMWARE_UPDATE_CHECK_TIME)
                checkTime = parameter.second;

        // And call the callback
        m_firmwareParametersListener->receiveParameters(repository, checkTime);
    };
    m_dataService.synchronizeParameters(deviceKey, parameters, callback);
}

const Protocol& FirmwareUpdateService::getProtocol()
{
    return m_protocol;
}

void FirmwareUpdateService::messageReceived(std::shared_ptr<Message> message)
{
    LOG(TRACE) << METHOD_INFO;

    // Check that the message is not null
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to process received message -> The message is null.";
        return;
    }
    if (m_firmwareInstaller == nullptr)
    {
        LOG(ERROR) << "Failed to process received message -> The firmware installer is null.";
        return;
    }

    // Look for the device this message is targeting
    auto type = m_protocol.getMessageType(*message);
    auto target = m_protocol.getDeviceKey(*message);
    LOG(TRACE) << "Received message '" << toString(type) << "' for target '" << target << "'.";

    // Parse the received message based on the type
    switch (type)
    {
    case MessageType::FIRMWARE_UPDATE_INSTALL:
    {
        if (auto parsedMessage = m_protocol.parseFirmwareUpdateInstall(message))
            onFirmwareInstall(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FirmwareUpdateInstall' message.";
        return;
    }
    case MessageType::FIRMWARE_UPDATE_ABORT:
    {
        if (auto parsedMessage = m_protocol.parseFirmwareUpdateAbort(message))
            onFirmwareAbort(target, *parsedMessage);
        else
            LOG(ERROR) << "Failed to parse 'FirmwareUpdateAbort' message.";
        return;
    }
    default:
        LOG(ERROR) << "Received a message of invalid type for this service.";
        return;
    }
}

void FirmwareUpdateService::onFirmwareInstall(const std::string& deviceKey, const FirmwareUpdateInstallMessage& message)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if there's already an installation session ongoing
    if (m_installation.find(deviceKey) != m_installation.cend() && m_installation[deviceKey])
    {
        LOG(WARN) << "Received 'FirmwareUpdateInstallMessage' but an installation is already ongoing.";
        return;
    }

    // Check with the installer
    auto status = m_firmwareInstaller->installFirmware(deviceKey, message.getFile());
    switch (status)
    {
    case InstallResponse::FAILED_TO_INSTALL:
        sendStatusMessage(deviceKey, FirmwareUpdateStatus::ERROR, FirmwareUpdateError::INSTALLATION_FAILED);
        return;
    case InstallResponse::NO_FILE:
        sendStatusMessage(deviceKey, FirmwareUpdateStatus::ERROR, FirmwareUpdateError::UNKNOWN_FILE);
        return;
    case InstallResponse::WILL_INSTALL:
        if (!storeSessionFile(deviceKey, m_firmwareInstaller->getFirmwareVersion(deviceKey)))
        {
            LOG(ERROR) << "Failed to store session file.";
            sendStatusMessage(deviceKey, FirmwareUpdateStatus::ERROR, FirmwareUpdateError::INSTALLATION_FAILED);
            return;
        }
        if (m_installation.find(deviceKey) == m_installation.cend())
            m_installation.emplace(deviceKey, true);
        else
            m_installation[deviceKey] = true;
        sendStatusMessage(deviceKey, FirmwareUpdateStatus::INSTALLING);
        return;
    case InstallResponse::INSTALLED:
        sendStatusMessage(deviceKey, FirmwareUpdateStatus::SUCCESS);
        break;
    }
}

void FirmwareUpdateService::onFirmwareAbort(const std::string& deviceKey,
                                            const FirmwareUpdateAbortMessage& /** message **/)
{
    LOG(TRACE) << METHOD_INFO;

    // Check if the session file is present
    if (m_installation.find(deviceKey) != m_installation.cend() && m_installation[deviceKey])
        m_firmwareInstaller->abortFirmwareInstall(deviceKey);
}

void FirmwareUpdateService::sendStatusMessage(const std::string& deviceKey, FirmwareUpdateStatus status,
                                              FirmwareUpdateError error)
{
    LOG(TRACE) << METHOD_INFO;

    // Create the status message
    auto statusMessage = FirmwareUpdateStatusMessage(status, error);
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, statusMessage));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to generate outbound FirmwareUpdateStatusMessage.";
        return;
    }
    m_connectivityService.publish(message);
}

void FirmwareUpdateService::queueStatusMessage(const std::string& deviceKey, FirmwareUpdateStatus status,
                                               FirmwareUpdateError error)
{
    LOG(TRACE) << METHOD_INFO;

    // Create the status message
    auto statusMessage = FirmwareUpdateStatusMessage(status, error);
    auto message = std::shared_ptr<Message>(m_protocol.makeOutboundMessage(deviceKey, statusMessage));
    if (message == nullptr)
    {
        LOG(ERROR) << "Failed to generate outbound FirmwareUpdateStatusMessage.";
        return;
    }
    m_queue.push(message);
}

bool FirmwareUpdateService::storeSessionFile(const std::string& deviceKey, const std::string& version)
{
    return FileSystemUtils::createFileWithContent(m_sessionFile + "_" + deviceKey, version);
}

void FirmwareUpdateService::deleteSessionFile(const std::string& deviceKey)
{
    FileSystemUtils::deleteFile(m_sessionFile + "_" + deviceKey);
}
}    // namespace connect
}    // namespace wolkabout

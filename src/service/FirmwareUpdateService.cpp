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

#ifndef FIRMWAREUPDATESERVICE_CPP
#define FIRMWAREUPDATESERVICE_CPP

#include "FirmwareUpdateService.h"
#include "FirmwareInstaller.h"
#include "UrlFileDownloader.h"
#include "WolkaboutFileDownloader.h"
#include "connectivity/ConnectivityService.h"
#include "model/FirmwareUpdateCommand.h"
#include "model/FirmwareUpdateResponse.h"
#include "model/Message.h"
#include "protocol/FirmwareUpdateProtocol.h"
#include "utilities/ConsoleLogger.h"
#include "utilities/FileSystemUtils.h"
#include "utilities/StringUtils.h"

#include <cassert>

namespace wolkabout
{
FirmwareUpdateService::FirmwareUpdateService(std::string deviceKey, FirmwareUpdateProtocol& protocol,
                                             const std::string& firmwareVersion, const std::string& downloadDirectory,
                                             std::uint_fast64_t maximumFirmwareSize,
                                             ConnectivityService& connectivityService,
                                             std::weak_ptr<WolkaboutFileDownloader> wolkDownloader,
                                             std::weak_ptr<UrlFileDownloader> urlDownloader,
                                             std::weak_ptr<FirmwareInstaller> firmwareInstaller)
: m_deviceKey{std::move(deviceKey)}
, m_protocol{protocol}
, m_currentFirmwareVersion{firmwareVersion}
, m_firmwareDownloadDirectory{downloadDirectory}
, m_maximumFirmwareSize{maximumFirmwareSize}
, m_connectivityService{connectivityService}
, m_wolkFileDownloader{wolkDownloader}
, m_urlFileDownloader{urlDownloader}
, m_firmwareInstaller{firmwareInstaller}
, m_idleState{new FirmwareUpdateService::IdleState(*this)}
, m_wolkDownloadState{new FirmwareUpdateService::WolkDownloadState(*this)}
, m_urlDownloadState{new FirmwareUpdateService::UrlDownloadState(*this)}
, m_readyState{new FirmwareUpdateService::ReadyState(*this)}
, m_installationState{new FirmwareUpdateService::InstallationState(*this)}
, m_firmwareFile{""}
, m_autoInstall{false}
, m_commandBuffer{new CommandBuffer()}
{
    m_currentState = m_idleState.get();
}

FirmwareUpdateService::~FirmwareUpdateService()
{
    if (m_executor && m_executor->joinable())
    {
        m_executor->join();
    }
}

void FirmwareUpdateService::messageReceived(std::shared_ptr<Message> message)
{
    assert(message);

    const std::string deviceKey = m_protocol.extractDeviceKeyFromChannel(message->getChannel());
    if (deviceKey.empty())
    {
        LOG(WARN) << "Unable to extract device key from channel: " << message->getChannel();
        return;
    }

    if (deviceKey != m_deviceKey)
    {
        LOG(WARN) << "Device key mismatch: " << message->getChannel();
        return;
    }

    if (m_protocol.isFirmwareUpdateMessage(*message))
    {
        auto command = m_protocol.makeFirmwareUpdateCommand(*message);
        if (!command)
        {
            LOG(WARN) << "Unable to parse message contents: " << message->getContent();
            return;
        }

        handleFirmwareUpdateCommand(*command);
    }
    else
    {
        LOG(WARN) << "Unable to parse message channel: " << message->getChannel();
    }
}

const Protocol& FirmwareUpdateService::getProtocol()
{
    return m_protocol;
}

void FirmwareUpdateService::handleFirmwareUpdateCommand(const FirmwareUpdateCommand& firmwareUpdateCommand)
{
    addToCommandBuffer([=] { m_currentState->handleFirmwareUpdateCommand(firmwareUpdateCommand); });
}

const std::string& FirmwareUpdateService::getFirmwareVersion() const
{
    return m_currentFirmwareVersion;
}

void FirmwareUpdateService::reportFirmwareUpdateResult()
{
    if (!FileSystemUtils::isFilePresent(FIRMWARE_VERSION_FILE))
    {
        return;
    }

    std::string firmwareVersion;
    FileSystemUtils::readFileContent(FIRMWARE_VERSION_FILE, firmwareVersion);

    StringUtils::removeTrailingWhitespace(firmwareVersion);

    if (m_currentFirmwareVersion != firmwareVersion)
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::COMPLETED});
    }
    else
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::INSTALLATION_FAILED});
    }

    FileSystemUtils::deleteFile(FIRMWARE_VERSION_FILE);
}

void FirmwareUpdateService::publishFirmwareVersion()
{
    const std::shared_ptr<Message> message = m_protocol.makeFromFirmwareVersion(m_deviceKey, m_currentFirmwareVersion);

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
}

void FirmwareUpdateService::IdleState::handleFirmwareUpdateCommand(const FirmwareUpdateCommand& firmwareUpdateCommand)
{
    switch (firmwareUpdateCommand.getType())
    {
    case FirmwareUpdateCommand::Type::FILE_UPLOAD:
    {
        auto name = firmwareUpdateCommand.getName();
        if (!name || name.value().empty())
        {
            m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                          FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
            return;
        }

        const auto size = firmwareUpdateCommand.getSize();
        if (!size || size.value() > m_service.m_maximumFirmwareSize || static_cast<uint_fast64_t>(size) == 0)
        {
            m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                          FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
            return;
        }

        const auto hash = firmwareUpdateCommand.getHash();
        if (!hash || hash.value().empty())
        {
            m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                          FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
            return;
        }

        if (m_service.m_wolkFileDownloader.lock())
        {
            m_service.m_currentState = m_service.m_wolkDownloadState.get();

            const auto autoInstall = firmwareUpdateCommand.getAutoInstall();
            m_service.m_autoInstall = autoInstall && autoInstall.value();

            const auto byteHash = ByteUtils::toByteArray(StringUtils::base64Decode(hash.value()));
            m_service.downloadFirmware(name.value(), size.value(), byteHash);
        }
        else
        {
            m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                          FirmwareUpdateResponse::ErrorCode::FILE_UPLOAD_DISABLED});
        }

        break;
    }
    case FirmwareUpdateCommand::Type::URL_DOWNLOAD:
    {
        const auto url = firmwareUpdateCommand.getUrl();
        if (!url || url.value().empty())
        {
            m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                          FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
            return;
        }

        if (m_service.m_urlFileDownloader.lock())
        {
            m_service.m_currentState = m_service.m_urlDownloadState.get();

            const auto autoInstall = firmwareUpdateCommand.getAutoInstall();
            m_service.m_autoInstall = autoInstall && autoInstall.value();

            m_service.downloadFirmware(url.value());
        }
        else
        {
            m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                          FirmwareUpdateResponse::ErrorCode::FILE_UPLOAD_DISABLED});
        }

        break;
    }
    case FirmwareUpdateCommand::Type::INSTALL:
    case FirmwareUpdateCommand::Type::ABORT:
    case FirmwareUpdateCommand::Type::UNKNOWN:
    default:
    {
        m_service.clear();
        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                      FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
    }
    }
}

void FirmwareUpdateService::WolkDownloadState::handleFirmwareUpdateCommand(
  const FirmwareUpdateCommand& firmwareUpdateCommand)
{
    switch (firmwareUpdateCommand.getType())
    {
    case FirmwareUpdateCommand::Type::ABORT:
    {
        if (auto downloader = m_service.m_wolkFileDownloader.lock())
        {
            downloader->abort();
        }

        m_service.clear();

        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ABORTED});

        break;
    }
    case FirmwareUpdateCommand::Type::INSTALL:
    case FirmwareUpdateCommand::Type::FILE_UPLOAD:
    case FirmwareUpdateCommand::Type::URL_DOWNLOAD:
    case FirmwareUpdateCommand::Type::UNKNOWN:
    default:
    {
        if (auto downloader = m_service.m_wolkFileDownloader.lock())
        {
            downloader->abort();
        }

        m_service.clear();
        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                      FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
    }
    }
}

void FirmwareUpdateService::UrlDownloadState::handleFirmwareUpdateCommand(
  const FirmwareUpdateCommand& firmwareUpdateCommand)
{
    switch (firmwareUpdateCommand.getType())
    {
    case FirmwareUpdateCommand::Type::ABORT:
    {
        if (auto downloader = m_service.m_urlFileDownloader.lock())
        {
            downloader->abort();
        }

        m_service.clear();

        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ABORTED});

        break;
    }
    case FirmwareUpdateCommand::Type::INSTALL:
    case FirmwareUpdateCommand::Type::FILE_UPLOAD:
    case FirmwareUpdateCommand::Type::URL_DOWNLOAD:
    case FirmwareUpdateCommand::Type::UNKNOWN:
    default:
    {
        if (auto downloader = m_service.m_urlFileDownloader.lock())
        {
            downloader->abort();
        }

        m_service.clear();
        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                      FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
    }
    }
}

void FirmwareUpdateService::ReadyState::handleFirmwareUpdateCommand(const FirmwareUpdateCommand& firmwareUpdateCommand)
{
    switch (firmwareUpdateCommand.getType())
    {
    case FirmwareUpdateCommand::Type::INSTALL:
    {
        m_service.install();

        break;
    }
    case FirmwareUpdateCommand::Type::ABORT:
    {
        if (!FileSystemUtils::deleteFile(m_service.m_firmwareFile))
        {
            // TODO log error
        }

        m_service.clear();

        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ABORTED});

        break;
    }
    case FirmwareUpdateCommand::Type::FILE_UPLOAD:
    case FirmwareUpdateCommand::Type::URL_DOWNLOAD:
    case FirmwareUpdateCommand::Type::UNKNOWN:
    default:
    {
        if (!FileSystemUtils::deleteFile(m_service.m_firmwareFile))
        {
            // TODO log error
        }

        m_service.clear();
        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                      FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
    }
    }
}

void FirmwareUpdateService::InstallationState::handleFirmwareUpdateCommand(
  const FirmwareUpdateCommand& firmwareUpdateCommand)
{
    switch (firmwareUpdateCommand.getType())
    {
    case FirmwareUpdateCommand::Type::ABORT:
    {
        if (m_service.m_executor && m_service.m_executor->joinable())
        {
            m_service.m_executor->join();
        }

        if (!FileSystemUtils::deleteFile(m_service.m_firmwareFile))
        {
            // TODO log error
        }

        m_service.clear();

        m_service.sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ABORTED});

        break;
    }
    case FirmwareUpdateCommand::Type::INSTALL:
    case FirmwareUpdateCommand::Type::FILE_UPLOAD:
    case FirmwareUpdateCommand::Type::URL_DOWNLOAD:
    case FirmwareUpdateCommand::Type::UNKNOWN:
    default:
    {
        break;
    }
    }
}

void FirmwareUpdateService::addToCommandBuffer(std::function<void()> command)
{
    m_commandBuffer->pushCommand(std::make_shared<std::function<void()>>(command));
}

void FirmwareUpdateService::sendResponse(const FirmwareUpdateResponse& response)
{
    std::shared_ptr<Message> message = m_protocol.makeMessage(m_deviceKey, response);

    if (!message)
    {
        LOG(WARN) << "Failed to create firmware update response";
        return;
    }

    if (!m_connectivityService.publish(message))
    {
        LOG(WARN) << "Failed to publish firmware update response";
        return;
    }
}

void FirmwareUpdateService::onFirmwareFileDownloadSuccess(const std::string& filePath)
{
    if (m_currentState == m_wolkDownloadState.get() || m_currentState == m_urlDownloadState.get())
    {
        m_currentState = m_readyState.get();
        m_firmwareFile = filePath;

        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::FILE_READY});

        if (m_autoInstall)
        {
            install();
        }
    }
}

void FirmwareUpdateService::onFirmwareFileDownloadFail(WolkaboutFileDownloader::Error errorCode)
{
    switch (errorCode)
    {
    case WolkaboutFileDownloader::Error::FILE_SYSTEM_ERROR:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::FILE_SYSTEM_ERROR});
        clear();
        break;
    }
    case WolkaboutFileDownloader::Error::RETRY_COUNT_EXCEEDED:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::RETRY_COUNT_EXCEEDED});
        clear();
        break;
    }
    case WolkaboutFileDownloader::Error::UNSUPPORTED_FILE_SIZE:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::UNSUPPORTED_FILE_SIZE});
        clear();
        break;
    }
    case WolkaboutFileDownloader::Error::UNSPECIFIED_ERROR:
    default:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
        clear();
        break;
    }
    }
}

void FirmwareUpdateService::onFirmwareFileDownloadFail(UrlFileDownloader::Error errorCode)
{
    switch (errorCode)
    {
    case UrlFileDownloader::Error::FILE_SYSTEM_ERROR:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::FILE_SYSTEM_ERROR});
        clear();
        break;
    }
    case UrlFileDownloader::Error::MALFORMED_URL:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::MALFORMED_URL});
        clear();
        break;
    }
    case UrlFileDownloader::Error::UNSUPPORTED_FILE_SIZE:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::UNSUPPORTED_FILE_SIZE});
        clear();
        break;
    }
    case UrlFileDownloader::Error::UNSPECIFIED_ERROR:
    default:
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                            FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR});
        clear();
        break;
    }
    }
}

void FirmwareUpdateService::downloadFirmware(const std::string& name, uint_fast64_t size, const ByteArray& hash)
{
    if (auto downloader = m_wolkFileDownloader.lock())
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::FILE_TRANSFER});

        if (m_executor && m_executor->joinable())
        {
            m_executor->join();
        }

        m_executor.reset(new std::thread([=] {
            downloader->download(name, size, hash, m_firmwareDownloadDirectory,
                                 [=](const std::string& firmwareFile) {    // onSuccess
                                     addToCommandBuffer([=] { onFirmwareFileDownloadSuccess(firmwareFile); });
                                 },
                                 [=](WolkaboutFileDownloader::Error errorCode) {    // onFail
                                     addToCommandBuffer([=] { onFirmwareFileDownloadFail(errorCode); });
                                 });
        }));
    }
}

void FirmwareUpdateService::downloadFirmware(const std::string& url)
{
    if (auto downloader = m_urlFileDownloader.lock())
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::FILE_TRANSFER});

        if (m_executor && m_executor->joinable())
        {
            m_executor->join();
        }

        m_executor.reset(new std::thread([=] {
            downloader->download(url, m_firmwareDownloadDirectory,
                                 [=](const std::string& firmwareFile) {    // onSuccess
                                     addToCommandBuffer([=] { onFirmwareFileDownloadSuccess(firmwareFile); });
                                 },
                                 [=](UrlFileDownloader::Error errorCode) {    // onFail
                                     addToCommandBuffer([=] { onFirmwareFileDownloadFail(errorCode); });
                                 });
        }));
    }
}

void FirmwareUpdateService::install()
{
    if (auto installer = m_firmwareInstaller.lock())
    {
        sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::INSTALLATION});

        if (m_executor && m_executor->joinable())
        {
            m_executor->join();
        }

        m_executor.reset(new std::thread([=] {
            if (!FileSystemUtils::createFileWithContent(FIRMWARE_VERSION_FILE, m_currentFirmwareVersion))
            {
                if (!FileSystemUtils::deleteFile(m_firmwareFile))
                {
                    // TODO log error
                }

                sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                    FirmwareUpdateResponse::ErrorCode::INSTALLATION_FAILED});

                clear();

                return;
            }

            if (!installer->install(m_firmwareFile))
            {
                if (!FileSystemUtils::deleteFile(m_firmwareFile))
                {
                    // TODO log error
                }

                sendResponse(FirmwareUpdateResponse{FirmwareUpdateResponse::Status::ERROR,
                                                    FirmwareUpdateResponse::ErrorCode::INSTALLATION_FAILED});

                clear();
            }
        }));
    }
}

void FirmwareUpdateService::clear()
{
    m_firmwareFile = "";
    m_autoInstall = false;
    m_currentState = m_idleState.get();
}

}    // namespace wolkabout

#endif    // FIRMWAREUPDATESERVICE_CPP

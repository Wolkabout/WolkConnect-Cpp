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

#include "wolk/WolkBuilder.h"

#include "core/InboundMessageHandler.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/connectivity/mqtt/MqttConnectivityService.h"
#include "core/persistence/inmemory/InMemoryPersistence.h"
#include "core/protocol/wolkabout/WolkaboutDataProtocol.h"
#include "core/protocol/wolkabout/WolkaboutFileManagementProtocol.h"
#include "core/protocol/wolkabout/WolkaboutFirmwareUpdateProtocol.h"
#include "wolk/InboundPlatformMessageHandler.h"
#include "wolk/Wolk.h"
#include "wolk/connectivity/mqtt/WolkPahoMqttClient.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/file_management/FileManagementService.h"
#include "wolk/service/firmware_update/FirmwareUpdateService.h"

#include <stdexcept>
#include <utility>

namespace wolkabout
{
WolkBuilder& WolkBuilder::host(const std::string& host)
{
    m_host = host;
    return *this;
}

WolkBuilder& WolkBuilder::caCertPath(const std::string& caCertPath)
{
    m_caCertPath = caCertPath;
    return *this;
}

WolkBuilder& WolkBuilder::feedUpdateHandler(
  const std::function<void(std::string, const std::map<std::uint64_t, std::vector<Reading>>)>& feedUpdateHandler)
{
    m_feedUpdateHandlerLambda = feedUpdateHandler;
    m_feedUpdateHandler.reset();
    return *this;
}

WolkBuilder& WolkBuilder::feedUpdateHandler(std::weak_ptr<FeedUpdateHandler> feedUpdateHandler)
{
    m_feedUpdateHandler = std::move(feedUpdateHandler);
    m_feedUpdateHandlerLambda = nullptr;
    return *this;
}

WolkBuilder& WolkBuilder::parameterHandler(
  const std::function<void(std::string, std::vector<Parameter>)>& parameterHandlerLambda)
{
    m_parameterHandlerLambda = parameterHandlerLambda;
    m_parameterHandler.reset();
    return *this;
}
WolkBuilder& WolkBuilder::parameterHandler(std::weak_ptr<ParameterHandler> parameterHandler)
{
    m_parameterHandler = std::move(parameterHandler);
    m_parameterHandlerLambda = nullptr;
    return *this;
}

WolkBuilder& WolkBuilder::withPersistence(std::shared_ptr<Persistence> persistence)
{
    m_persistence = std::move(persistence);
    return *this;
}

WolkBuilder& WolkBuilder::withDataProtocol(std::unique_ptr<DataProtocol> protocol)
{
    m_dataProtocol = std::move(protocol);
    return *this;
}

WolkBuilder& WolkBuilder::withFileTransfer(const std::string& fileDownloadLocation, std::uint64_t maxPacketSize)
{
    if (m_firmwareUpdateProtocol == nullptr)
        m_fileManagementProtocol =
          std::unique_ptr<WolkaboutFileManagementProtocol>(new wolkabout::WolkaboutFileManagementProtocol);
    m_fileDownloadDirectory = fileDownloadLocation;
    m_fileTransferEnabled = true;
    m_fileTransferUrlEnabled = false;
    m_fileDownloader = nullptr;
    m_maxPacketSize = maxPacketSize;
    return *this;
}

WolkBuilder& WolkBuilder::withFileURLDownload(const std::string& fileDownloadLocation,
                                              std::shared_ptr<FileDownloader> fileDownloader, bool transferEnabled,
                                              std::uint64_t maxPacketSize)
{
    if (m_firmwareUpdateProtocol == nullptr)
        m_fileManagementProtocol =
          std::unique_ptr<WolkaboutFileManagementProtocol>(new wolkabout::WolkaboutFileManagementProtocol);
    m_fileDownloadDirectory = fileDownloadLocation;
    m_fileTransferEnabled = transferEnabled;
    m_fileTransferUrlEnabled = true;
    m_fileDownloader = std::move(fileDownloader);
    m_maxPacketSize = maxPacketSize;
    return *this;
}

WolkBuilder& WolkBuilder::withFileListener(const std::shared_ptr<FileListener>& fileListener)
{
    m_fileListener = fileListener;
    return *this;
}

WolkBuilder& WolkBuilder::withFirmwareUpdate(std::shared_ptr<FirmwareInstaller> firmwareInstaller,
                                             const std::string& workingDirectory)
{
    if (m_firmwareUpdateProtocol == nullptr)
        m_firmwareUpdateProtocol =
          std::unique_ptr<WolkaboutFirmwareUpdateProtocol>(new wolkabout::WolkaboutFirmwareUpdateProtocol);
    m_firmwareParametersListener = nullptr;
    m_firmwareInstaller = std::move(firmwareInstaller);
    m_workingDirectory = workingDirectory;
    return *this;
}

WolkBuilder& WolkBuilder::withFirmwareUpdate(std::shared_ptr<FirmwareParametersListener> firmwareParametersListener,
                                             const std::string& workingDirectory)
{
    if (m_firmwareUpdateProtocol == nullptr)
        m_firmwareUpdateProtocol =
          std::unique_ptr<WolkaboutFirmwareUpdateProtocol>(new wolkabout::WolkaboutFirmwareUpdateProtocol);
    m_firmwareInstaller = nullptr;
    m_firmwareParametersListener = std::move(firmwareParametersListener);
    m_workingDirectory = workingDirectory;
    return *this;
}

std::unique_ptr<Wolk> WolkBuilder::build()
{
    if (m_device.getKey().empty())
    {
        throw std::logic_error("No device key present.");
    }

    auto wolk = std::unique_ptr<Wolk>(new Wolk(m_device));

    wolk->m_dataProtocol = std::move(m_dataProtocol);

    wolk->m_persistence = m_persistence;

    auto mqttClient = std::make_shared<WolkPahoMqttClient>();
    wolk->m_connectivityService = std::unique_ptr<MqttConnectivityService>(
      new MqttConnectivityService(mqttClient, m_device.getKey(), m_device.getPassword(), m_host, m_caCertPath));

    wolk->m_inboundMessageHandler =
      std::unique_ptr<InboundMessageHandler>(new InboundPlatformMessageHandler(m_device.getKey()));

    auto wolkRaw = wolk.get();
    wolk->m_connectivityManager = std::make_shared<Wolk::ConnectivityFacade>(*wolk->m_inboundMessageHandler,
                                                                             [wolkRaw]
                                                                             {
                                                                                 wolkRaw->notifyDisonnected();
                                                                                 wolkRaw->connect();
                                                                             });

    wolk->m_feedUpdateHandlerLambda = m_feedUpdateHandlerLambda;
    wolk->m_feedUpdateHandler = m_feedUpdateHandler;

    wolk->m_parameterLambda = m_parameterHandlerLambda;
    wolk->m_parameterHandler = m_parameterHandler;

    // Data service
    wolk->m_dataService = std::make_shared<DataService>(
      *wolk->m_dataProtocol, *wolk->m_persistence, *wolk->m_connectivityService,
      [wolkRaw](const std::string& deviceKey, const std::map<std::uint64_t, std::vector<Reading>>& readings)
      { wolkRaw->handleFeedUpdateCommand(deviceKey, readings); },
      [wolkRaw](const std::string& deviceKey, const std::vector<Parameter>& parameters)
      { wolkRaw->handleParameterCommand(deviceKey, parameters); });
    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);

    // Check if the file management should be engaged
    if (m_fileManagementProtocol != nullptr)
    {
        // Create the File Management service
        wolk->m_fileManagementProtocol = std::move(m_fileManagementProtocol);
        if (m_fileTransferUrlEnabled && m_fileDownloader == nullptr)
        {
            m_fileDownloader = std::make_shared<HTTPFileDownloader>();
        }
        wolk->m_fileDownloader = std::move(m_fileDownloader);
        wolk->m_fileListener = std::move(m_fileListener);

        wolk->m_fileManagementService = std::make_shared<FileManagementService>(
          wolk->m_device.getKey(), *wolk->m_connectivityService, *wolk->m_dataService, *wolk->m_fileManagementProtocol,
          m_fileDownloadDirectory, m_fileTransferEnabled, m_fileTransferUrlEnabled, m_maxPacketSize,
          wolk->m_fileDownloader, wolk->m_fileListener);

        // Trigger the on build and add the listener for MQTT messages
        wolk->m_fileManagementService->createFolder();
        wolk->m_inboundMessageHandler->addListener(wolk->m_fileManagementService);
    }

    // Set the parameters about the FileTransfer
    wolk->m_dataService->updateParameter(
      m_device.getKey(), {ParameterName::FILE_TRANSFER_PLATFORM_ENABLED, m_fileTransferEnabled ? "true" : "false"});
    wolk->m_dataService->updateParameter(
      m_device.getKey(), {ParameterName::FILE_TRANSFER_URL_ENABLED, m_fileTransferUrlEnabled ? "true" : "false"});

    // Check if the firmware update should be engaged
    if (m_firmwareUpdateProtocol != nullptr)
    {
        // Create the Firmware Update service
        wolk->m_firmwareUpdateProtocol = std::move(m_firmwareUpdateProtocol);

        // Build based on the module we have
        if (m_firmwareInstaller != nullptr)
        {
            wolk->m_firmwareUpdateService = std::make_shared<FirmwareUpdateService>(
              wolk->m_device.getKey(), *wolk->m_connectivityService, *wolk->m_dataService, m_firmwareInstaller,
              *wolk->m_firmwareUpdateProtocol, m_workingDirectory);
        }
        else if (m_firmwareParametersListener != nullptr)
        {
            wolk->m_firmwareUpdateService = std::make_shared<FirmwareUpdateService>(
              wolk->m_device.getKey(), *wolk->m_connectivityService, *wolk->m_dataService, m_firmwareParametersListener,
              *wolk->m_firmwareUpdateProtocol, m_workingDirectory);
        }

        // And set it all up
        wolk->m_firmwareUpdateService->loadState();
        wolk->m_inboundMessageHandler->addListener(wolk->m_firmwareUpdateService);
    }

    // Set the parameters about the FirmwareUpdate
    wolk->m_dataService->updateParameter(m_device.getKey(), {ParameterName::FIRMWARE_UPDATE_ENABLED,
                                                             m_firmwareUpdateProtocol != nullptr ? "true" : "false"});
    {
        auto firmwareVersion = std::string{};
        if (m_firmwareInstaller != nullptr)
            firmwareVersion = m_firmwareInstaller->getFirmwareVersion();
        else if (m_firmwareParametersListener != nullptr)
            firmwareVersion = m_firmwareParametersListener->getFirmwareVersion();
        wolk->m_dataService->updateParameter(m_device.getKey(), {ParameterName::FIRMWARE_VERSION, firmwareVersion});
    }

    wolk->m_connectivityService->setListener(wolk->m_connectivityManager);

    return wolk;
}

wolkabout::WolkBuilder::operator std::unique_ptr<Wolk>()
{
    return build();
}

WolkBuilder::WolkBuilder(Device device)
: m_host{WOLK_DEMO_HOST}
, m_caCertPath{TRUST_STORE}
, m_device{std::move(device)}
, m_persistence{new InMemoryPersistence()}
, m_dataProtocol(new WolkaboutDataProtocol())
, m_fileTransferEnabled(false)
, m_fileTransferUrlEnabled(false)
, m_maxPacketSize{0}
{
}
}    // namespace wolkabout

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

#include "WolkBuilder.h"
#include "ActuationHandler.h"
#include "ActuatorStatusProvider.h"
#include "FileHandler.h"
#include "InboundMessageHandler.h"
#include "OutboundDataService.h"
#include "Wolk.h"
#include "connectivity/ConnectivityService.h"
#include "connectivity/json/JsonSingleOutboundMessageFactory.h"
#include "connectivity/mqtt/MqttConnectivityService.h"
#include "connectivity/mqtt/PahoMqttClient.h"
#include "model/Device.h"
#include "model/FirmwareUpdateCommand.h"
#include "persistence/Persistence.h"
#include "persistence/inmemory/InMemoryPersistence.h"
#include "service/FileDownloadService.h"
#include "service/FirmwareUpdateService.h"
#include "service/KeepAliveService.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace wolkabout
{
WolkBuilder& WolkBuilder::host(const std::string& host)
{
    m_host = host;
    return *this;
}

WolkBuilder& WolkBuilder::actuationHandler(
  const std::function<void(const std::string&, const std::string&)>& actuationHandler)
{
    m_actuationHandlerLambda = actuationHandler;
    m_actuationHandler.reset();
    return *this;
}

WolkBuilder& WolkBuilder::actuationHandler(std::weak_ptr<ActuationHandler> actuationHandler)
{
    m_actuationHandler = actuationHandler;
    m_actuationHandlerLambda = nullptr;
    return *this;
}

WolkBuilder& WolkBuilder::actuatorStatusProvider(
  const std::function<ActuatorStatus(const std::string&)>& actuatorStatusProvider)
{
    m_actuatorStatusProviderLambda = actuatorStatusProvider;
    m_actuatorStatusProvider.reset();
    return *this;
}

WolkBuilder& WolkBuilder::actuatorStatusProvider(std::weak_ptr<ActuatorStatusProvider> actuatorStatusProvider)
{
    m_actuatorStatusProvider = actuatorStatusProvider;
    m_actuatorStatusProviderLambda = nullptr;
    return *this;
}

WolkBuilder& WolkBuilder::withPersistence(std::shared_ptr<Persistence> persistence)
{
    m_persistence = persistence;
    return *this;
}

WolkBuilder& WolkBuilder::withFirmwareUpdate(const std::string& firmwareVersion,
                                             std::weak_ptr<FirmwareInstaller> installer,
                                             const std::string& firmwareDownloadDirectory,
                                             uint_fast64_t maxFirmwareFileSize,
                                             std::uint_fast64_t maxFirmwareFileChunkSize)
{
    return withFirmwareUpdate(firmwareVersion, installer, firmwareDownloadDirectory, maxFirmwareFileSize,
                              maxFirmwareFileChunkSize, std::weak_ptr<UrlFileDownloader>());
}

WolkBuilder& WolkBuilder::withFirmwareUpdate(const std::string& firmwareVersion,
                                             std::weak_ptr<FirmwareInstaller> installer,
                                             const std::string& firmwareDownloadDirectory,
                                             uint_fast64_t maxFirmwareFileSize,
                                             std::uint_fast64_t maxFirmwareFileChunkSize,
                                             std::weak_ptr<UrlFileDownloader> urlDownloader)
{
    m_firmwareVersion = firmwareVersion;
    m_firmwareDownloadDirectory = firmwareDownloadDirectory;
    m_maxFirmwareFileSize = maxFirmwareFileSize;
    m_maxFirmwareFileChunkSize = maxFirmwareFileChunkSize;
    m_firmwareInstaller = installer;
    m_urlFileDownloader = urlDownloader;
    return *this;
}

WolkBuilder& WolkBuilder::withoutInternalPing()
{
    m_pingEnabled = false;
    return *this;
}

std::unique_ptr<Wolk> WolkBuilder::build() const
{
    if (m_device.getDeviceKey().empty())
    {
        throw std::logic_error("No device key present.");
    }

    if (m_device.getActuatorReferences().size() != 0)
    {
        if (m_actuationHandler.lock() == nullptr && m_actuationHandlerLambda == nullptr)
        {
            throw std::logic_error("Actuation handler not set.");
        }

        if (m_actuatorStatusProvider.lock() == nullptr && m_actuatorStatusProviderLambda == nullptr)
        {
            throw std::logic_error("Actuator status provider not set.");
        }
    }

    auto outboundMessageFactory =
      std::make_shared<JsonSingleOutboundMessageFactory>(m_device.getDeviceKey(), m_device.getSensorDelimiters());

    auto mqttClient = std::make_shared<PahoMqttClient>();
    auto connectivityService = std::make_shared<MqttConnectivityService>(mqttClient, m_device, m_host);

    auto inboundMessageHandler = std::make_shared<InboundMessageHandler>(m_device);
    auto outboundDataServiceHandler =
      std::make_shared<OutboundDataService>(m_device, *outboundMessageFactory, *connectivityService);

    auto wolk = std::unique_ptr<Wolk>(
      new Wolk(connectivityService, m_persistence, inboundMessageHandler, outboundDataServiceHandler, m_device));

    wolk->m_outboundMessageFactory = outboundMessageFactory;

    wolk->m_actuationHandlerLambda = m_actuationHandlerLambda;
    wolk->m_actuationHandler = m_actuationHandler;

    wolk->m_actuatorStatusProviderLambda = m_actuatorStatusProviderLambda;
    wolk->m_actuatorStatusProvider = m_actuatorStatusProvider;

    wolk->m_fileDownloadService = std::make_shared<FileDownloadService>(
      m_maxFirmwareFileSize, m_maxFirmwareFileChunkSize, std::unique_ptr<FileHandler>(new FileHandler()),
      outboundDataServiceHandler);

    if (m_firmwareInstaller.lock() != nullptr)
    {
        wolk->m_firmwareUpdateService = std::make_shared<FirmwareUpdateService>(
          m_firmwareVersion, m_firmwareDownloadDirectory, m_maxFirmwareFileSize, outboundDataServiceHandler,
          wolk->m_fileDownloadService, m_urlFileDownloader, m_firmwareInstaller);
    }

    inboundMessageHandler->setActuatorCommandHandler(
      [&](const ActuatorCommand& actuatorCommand) -> void { wolk->handleActuatorCommand(actuatorCommand); });

    std::weak_ptr<FileDownloadService> fileDownloadService_weak{wolk->m_fileDownloadService};
    inboundMessageHandler->setBinaryDataHandler([=](const BinaryData& binaryData) -> void {
        if (auto handler = fileDownloadService_weak.lock())
        {
            handler->handleBinaryData(binaryData);
        }
    });

    std::weak_ptr<FirmwareUpdateService> firmwareUpdateService_weak{wolk->m_firmwareUpdateService};
    inboundMessageHandler->setFirmwareUpdateCommandHandler(
      [=](const FirmwareUpdateCommand& firmwareUpdateCommand) -> void {
          if (auto handler = firmwareUpdateService_weak.lock())
          {
              handler->handleFirmwareUpdateCommand(firmwareUpdateCommand);
          }
      });

    if (m_pingEnabled)
    {
        wolk->m_keepAliveService = std::make_shared<KeepAliveService>(
          *wolk->m_outboundMessageFactory, *wolk->m_connectivityService, Wolk::KEEP_ALIVE_INTERVAL);

        std::weak_ptr<KeepAliveService> keepAliveService_weak{wolk->m_keepAliveService};
        inboundMessageHandler->setPongHandler([=]() -> void {
            if (auto handler = keepAliveService_weak.lock())
            {
                handler->handlePong();
            }
        });
    }

    connectivityService->setListener(std::dynamic_pointer_cast<ConnectivityServiceListener>(inboundMessageHandler));

    return wolk;
}

wolkabout::WolkBuilder::operator std::unique_ptr<Wolk>() const
{
    return build();
}

WolkBuilder::WolkBuilder(Device device)
: m_host{WOLK_DEMO_HOST}
, m_device{std::move(device)}
, m_persistence{new InMemoryPersistence()}
, m_firmwareVersion{""}
, m_firmwareDownloadDirectory{""}
, m_maxFirmwareFileSize{0}
, m_maxFirmwareFileChunkSize{0}
, m_pingEnabled{true}
{
}
}    // namespace wolkabout

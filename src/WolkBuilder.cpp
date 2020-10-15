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
#include "InboundMessageHandler.h"
#include "InboundPlatformMessageHandler.h"
#include "Wolk.h"
#include "connectivity/ConnectivityService.h"
#include "connectivity/mqtt/MqttConnectivityService.h"
#include "connectivity/mqtt/WolkPahoMqttClient.h"
#include "persistence/inmemory/InMemoryPersistence.h"
#include "protocol/json/JsonDFUProtocol.h"
#include "protocol/json/JsonDownloadProtocol.h"
#include "protocol/json/JsonProtocol.h"
#include "protocol/json/JsonSingleReferenceProtocol.h"
#include "protocol/json/JsonStatusProtocol.h"
#include "repository/SQLiteFileRepository.h"
#include "service/data/DataService.h"
#include "service/file/FileDownloadService.h"
#include "service/firmware/FirmwareUpdateService.h"
#include "service/keep_alive/KeepAliveService.h"

#include <stdexcept>

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

WolkBuilder& WolkBuilder::configurationHandler(
  std::function<void(const std::vector<ConfigurationItem>& configuration)> configurationHandler)
{
    m_configurationHandlerLambda = configurationHandler;
    m_configurationHandler.reset();
    return *this;
}

wolkabout::WolkBuilder& WolkBuilder::configurationHandler(std::weak_ptr<ConfigurationHandler> configurationHandler)
{
    m_configurationHandler = configurationHandler;
    m_configurationHandlerLambda = nullptr;
    return *this;
}

WolkBuilder& WolkBuilder::configurationProvider(std::function<std::vector<ConfigurationItem>()> configurationProvider)
{
    m_configurationProviderLambda = configurationProvider;
    m_configurationProvider.reset();
    return *this;
}

wolkabout::WolkBuilder& WolkBuilder::configurationProvider(std::weak_ptr<ConfigurationProvider> configurationProvider)
{
    m_configurationProvider = configurationProvider;
    m_configurationProviderLambda = nullptr;
    return *this;
}

WolkBuilder& WolkBuilder::withPersistence(std::shared_ptr<Persistence> persistence)
{
    m_persistence = persistence;
    return *this;
}

WolkBuilder& WolkBuilder::withDataProtocol(std::unique_ptr<DataProtocol> protocol)
{
    m_dataProtocol.reset(protocol.release());
    return *this;
}

WolkBuilder& WolkBuilder::withFileManagement(const std::string& fileDownloadDirectory, std::uint64_t maxPacketSize)
{
    return withFileManagement(fileDownloadDirectory, maxPacketSize, nullptr);
}

WolkBuilder& WolkBuilder::withFileManagement(const std::string& fileDownloadDirectory, std::uint64_t maxPacketSize,
                                             std::shared_ptr<UrlFileDownloader> urlDownloader)
{
    m_fileDownloadDirectory = fileDownloadDirectory;
    m_maxPacketSize = maxPacketSize;
    m_urlFileDownloader = urlDownloader;
    m_fileManagementEnabled = true;
    return *this;
}

WolkBuilder& WolkBuilder::withFirmwareUpdate(std::shared_ptr<FirmwareInstaller> installer,
                                             std::shared_ptr<FirmwareVersionProvider> provider)
{
    m_firmwareInstaller = installer;
    m_firmwareVersionProvider = provider;
    return *this;
}

WolkBuilder& WolkBuilder::withoutKeepAlive()
{
    m_keepAliveEnabled = false;
    return *this;
}

std::unique_ptr<Wolk> WolkBuilder::build()
{
    if (m_device.getKey().empty())
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

    if ((m_configurationHandlerLambda == nullptr && m_configurationProviderLambda != nullptr) ||
        (m_configurationHandlerLambda != nullptr && m_configurationProviderLambda == nullptr))
    {
        throw std::logic_error("Both ConfigurationPRovider and ConfigurationHandler must be set.");
    }

    if ((m_configurationHandler.expired() && !m_configurationProvider.expired()) ||
        (!m_configurationHandler.expired() && m_configurationProvider.expired()))
    {
        throw std::logic_error("Both ConfigurationPRovider and ConfigurationHandler must be set.");
    }

    auto wolk = std::unique_ptr<Wolk>(new Wolk(m_device));

    wolk->m_dataProtocol.reset(m_dataProtocol.release());
    wolk->m_fileDownloadProtocol = std::unique_ptr<JsonDownloadProtocol>(new JsonDownloadProtocol(false));
    wolk->m_firmwareUpdateProtocol = std::unique_ptr<JsonDFUProtocol>(new JsonDFUProtocol(false));
    wolk->m_statusProtocol = std::unique_ptr<StatusProtocol>(new JsonStatusProtocol());

    wolk->m_persistence = m_persistence;

    auto mqttClient = std::make_shared<WolkPahoMqttClient>();
    wolk->m_connectivityService = std::unique_ptr<MqttConnectivityService>(
      new MqttConnectivityService(mqttClient, m_device.getKey(), m_device.getPassword(), m_host, TRUST_STORE));

    wolk->m_inboundMessageHandler =
      std::unique_ptr<InboundMessageHandler>(new InboundPlatformMessageHandler(m_device.getKey()));

    wolk->m_connectivityManager = std::make_shared<Wolk::ConnectivityFacade>(*wolk->m_inboundMessageHandler, [&] {
        wolk->notifyDisonnected();
        wolk->connect();
    });

    wolk->m_actuationHandlerLambda = m_actuationHandlerLambda;
    wolk->m_actuationHandler = m_actuationHandler;

    wolk->m_configurationHandlerLambda = m_configurationHandlerLambda;
    wolk->m_configurationHandler = m_configurationHandler;

    wolk->m_configurationProviderLambda = m_configurationProviderLambda;
    wolk->m_configurationProvider = m_configurationProvider;

    wolk->m_actuatorStatusProviderLambda = m_actuatorStatusProviderLambda;
    wolk->m_actuatorStatusProvider = m_actuatorStatusProvider;

    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);

    // Data service
    wolk->m_dataService = std::make_shared<DataService>(
      wolk->m_device.getKey(), *wolk->m_dataProtocol, *wolk->m_persistence, *wolk->m_connectivityService,
      [&](const std::string& reference, const std::string& value) { wolk->handleActuatorSetCommand(reference, value); },
      [&](const std::string& reference) { wolk->handleActuatorGetCommand(reference); },
      [&](const ConfigurationSetCommand& command) { wolk->handleConfigurationSetCommand(command); },
      [&]() { wolk->handleConfigurationGetCommand(); });

    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);

    // Setup file repository
    wolk->m_fileRepository.reset(new SQLiteFileRepository(DATABASE));

    // File download service
    wolk->m_fileDownloadService = std::make_shared<FileDownloadService>(
      wolk->m_device.getKey(), *wolk->m_fileDownloadProtocol, m_fileDownloadDirectory, m_maxPacketSize,
      *wolk->m_connectivityService, *wolk->m_fileRepository, m_urlFileDownloader);

    wolk->m_inboundMessageHandler->addListener(wolk->m_fileDownloadService);

    // Firmware update service
    if (m_firmwareInstaller && m_firmwareVersionProvider)
    {
        if (!m_fileManagementEnabled)
        {
            throw std::logic_error("File management must be enabled in order to use firmware update.");
        }

        wolk->m_firmwareUpdateService = std::make_shared<FirmwareUpdateService>(
          wolk->m_device.getKey(), *wolk->m_firmwareUpdateProtocol, *wolk->m_fileRepository, m_firmwareInstaller,
          m_firmwareVersionProvider, *wolk->m_connectivityService);

        wolk->m_inboundMessageHandler->addListener(wolk->m_firmwareUpdateService);
    }

    if (m_keepAliveEnabled)
    {
        wolk->m_keepAliveService = std::make_shared<KeepAliveService>(
          wolk->m_device.getKey(), *wolk->m_statusProtocol, *wolk->m_connectivityService, Wolk::KEEP_ALIVE_INTERVAL);

        // Do not add listener as pong message is not handled
        wolk->m_inboundMessageHandler->addListener(wolk->m_keepAliveService);
    }

    wolk->m_connectivityService->setListener(wolk->m_connectivityManager);

    return wolk;
}

wolkabout::WolkBuilder::operator std::unique_ptr<Wolk>()
{
    return build();
}

WolkBuilder::~WolkBuilder() = default;

WolkBuilder::WolkBuilder(WolkBuilder&&) = default;

WolkBuilder::WolkBuilder(Device device)
: m_host{WOLK_DEMO_HOST}
, m_device{std::move(device)}
, m_persistence{new InMemoryPersistence()}
, m_dataProtocol{new JsonProtocol()}
, m_maxPacketSize{0}
, m_fileDownloadDirectory{""}
, m_firmwareInstaller{nullptr}
, m_firmwareVersionProvider{nullptr}
, m_keepAliveEnabled{true}
{
}
}    // namespace wolkabout

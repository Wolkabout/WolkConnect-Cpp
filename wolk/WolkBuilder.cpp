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

#include "core/InboundMessageHandler.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/connectivity/mqtt/MqttConnectivityService.h"
#include "core/persistence/inmemory/InMemoryPersistence.h"
#include "core/protocol/wolkabout/WolkaboutDataProtocol.h"
#include "core/protocol/wolkabout/WolkaboutFileManagementProtocol.h"
#include "wolk/connectivity/mqtt/WolkPahoMqttClient.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/file_management/FileManagementService.h"
#include "wolk/InboundPlatformMessageHandler.h"
#include "wolk/Wolk.h"
#include "wolk/WolkBuilder.h"

#include <Poco/Crypto/CipherKey.h>
#include <Poco/Util/ServerApplication.h>
#include <mqtt/async_client.h>
#include <stdexcept>
#include <utility>

namespace wolkabout
{
void randoMethod()
{
    // Take a cipher key
    auto key = Poco::Crypto::CipherKey("aes-256");
    Poco::Util::ServerApplication app{};
}

WolkBuilder& WolkBuilder::host(const std::string& host)
{
    m_host = host;
    return *this;
}

WolkBuilder& WolkBuilder::ca_cert_path(const std::string& ca_cert_path)
{
    m_ca_cert_path = ca_cert_path;

    return *this;
}

WolkBuilder& WolkBuilder::feedUpdateHandler(
  const std::function<void(const std::map<std::uint64_t, std::vector<Reading>>)>& feedUpdateHandler)
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

WolkBuilder& WolkBuilder::withFileManagement(const std::string& fileDownloadLocation, bool fileTransferEnabled,
                                             bool fileTransferUrlEnabled, std::uint64_t maxPacketSize)
{
    m_fileManagementProtocol.reset(new wolkabout::WolkaboutFileManagementProtocol);
    m_fileDownloadDirectory = fileDownloadLocation;
    m_fileTransferEnabled = fileTransferEnabled;
    m_fileTransferUrlEnabled = fileTransferUrlEnabled;
    m_maxPacketSize = maxPacketSize;
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
      new MqttConnectivityService(mqttClient, m_device.getKey(), m_device.getPassword(), m_host, m_ca_cert_path));

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

    // Data service
    wolk->m_dataService = std::make_shared<DataService>(
      wolk->m_device.getKey(), *wolk->m_dataProtocol, *wolk->m_persistence, *wolk->m_connectivityService,
      [wolkRaw](const std::map<std::uint64_t, std::vector<Reading>>& readings)
      { wolkRaw->handleFeedUpdateCommand(readings); },
      [wolkRaw](const std::vector<Parameter>& parameters) { wolkRaw->handleParameterCommand(parameters); });
    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);

    // Check if the file management should be engaged
    if (m_fileManagementProtocol != nullptr)
    {
        // Create the File Management service
        wolk->m_fileManagementProtocol = std::move(m_fileManagementProtocol);
        wolk->m_fileManagementService = std::make_shared<FileManagementService>(
          wolk->m_device.getKey(), *wolk->m_connectivityService, *wolk->m_dataService, *wolk->m_fileManagementProtocol,
          m_fileDownloadDirectory, m_fileTransferEnabled, m_fileTransferUrlEnabled, m_maxPacketSize);
        wolk->m_fileManagementService->onBuild();
        wolk->m_inboundMessageHandler->addListener(wolk->m_fileManagementService);
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
, m_ca_cert_path{TRUST_STORE}
, m_device{std::move(device)}
, m_persistence{new InMemoryPersistence()}
, m_dataProtocol(new WolkaboutDataProtocol())
, m_fileTransferEnabled(false)
, m_fileTransferUrlEnabled(false)
, m_maxPacketSize{0}
{
}
}    // namespace wolkabout

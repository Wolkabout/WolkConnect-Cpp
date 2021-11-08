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

#include "core/InboundMessageHandler.h"
#include "InboundPlatformMessageHandler.h"
#include "Wolk.h"
#include "core/connectivity/ConnectivityService.h"
#include "core/connectivity/mqtt/MqttConnectivityService.h"
#include "core/persistence/inmemory/InMemoryPersistence.h"
#include "core/protocol/WolkaboutDataProtocol.h"
#include "connectivity/mqtt/WolkPahoMqttClient.h"
#include "service/data/DataService.h"

#include <stdexcept>

namespace wolkabout
{
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
  const std::function<void(const std::map<unsigned long long int, std::vector<Reading>>)>& feedUpdateHandler)
{
    m_feedUpdateHandlerLambda = feedUpdateHandler;
    m_feedUpdateHandler.reset();
    return *this;
}

WolkBuilder& WolkBuilder::feedUpdateHandler(std::weak_ptr<FeedUpdateHandler> feedUpdateHandler)
{
    m_feedUpdateHandler = feedUpdateHandler;
    m_feedUpdateHandlerLambda = nullptr;
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

std::unique_ptr<Wolk> WolkBuilder::build()
{
    if (m_device.getKey().empty())
    {
        throw std::logic_error("No device key present.");
    }

    auto wolk = std::unique_ptr<Wolk>(new Wolk(m_device));

    wolk->m_dataProtocol.reset(m_dataProtocol.release());

    wolk->m_persistence = m_persistence;

    auto mqttClient = std::make_shared<WolkPahoMqttClient>();
    wolk->m_connectivityService = std::unique_ptr<MqttConnectivityService>(
      new MqttConnectivityService(mqttClient, m_device.getKey(), m_device.getPassword(), m_host, m_ca_cert_path));

    wolk->m_inboundMessageHandler =
      std::unique_ptr<InboundMessageHandler>(new InboundPlatformMessageHandler(m_device.getKey()));

    wolk->m_connectivityManager = std::make_shared<Wolk::ConnectivityFacade>(*wolk->m_inboundMessageHandler, [&] {
        wolk->notifyDisonnected();
        wolk->connect();
    });

    wolk->m_feedUpdateHandlerLambda = m_feedUpdateHandlerLambda;
    wolk->m_feedUpdateHandler = m_feedUpdateHandler;

    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);

    // Data service
    wolk->m_dataService = std::make_shared<DataService>(
      wolk->m_device.getKey(), *wolk->m_dataProtocol, *wolk->m_persistence, *wolk->m_connectivityService,
      [&](const std::map<unsigned long long int, std::vector<Reading>> readings)
      {
          wolk->handleFeedUpdateCommand(readings);
      },
      [&](const std::vector<Parameters> parameters)
      {
        wolk->handleParameterCommand(parameters);
      });

    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);

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
, m_ca_cert_path{TRUST_STORE}
, m_device{std::move(device)}
, m_persistence{new InMemoryPersistence()}
, m_dataProtocol(new WolkaboutDataProtocol())
, m_maxPacketSize{0}
{
}
}    // namespace wolkabout

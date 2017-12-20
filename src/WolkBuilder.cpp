/*
 * Copyright 2017 WolkAbout Technology s.r.o.
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
#include "Wolk.h"
#include "connectivity/ConnectivityService.h"
#include "connectivity/mqtt/MqttConnectivityService.h"
#include "connectivity/mqtt/PahoMqttClient.h"
#include "model/Device.h"
#include "InboundMessageHandler.h"
#include "persistence/Persistence.h"
#include "persistence/inmemory/InMemoryPersistence.h"
#include "ServiceCommandHandler.h"
#include "model/FirmwareUpdateCommand.h"
#include "OutboundDataService.h"
#include "FileDownloadMqttService.h"
#include "FileDownloadUrlService.h"
#include "FirmwareUpdateService.h"

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

WolkBuilder& WolkBuilder::withDirectFileDownload()
{
	m_directFileDownloadEnabled = true;
	return *this;
}

WolkBuilder& WolkBuilder::withUrlFileDownload(std::weak_ptr<UrlFileDownloader> downloader)
{
	m_urlFileDownloader = downloader;
	return *this;
}

WolkBuilder& WolkBuilder::withFirmwareUpdate(std::weak_ptr<FirmwareInstaller> installer)
{
	m_firmwareInstaller = installer;
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

	if(m_firmwareInstaller.lock() != nullptr)
	{
		if(!(m_directFileDownloadEnabled || m_urlFileDownloader.lock() != nullptr))
		{
			throw std::logic_error("At least one file download method must be enabled for firmware update");
		}
	}

    auto mqttClient = std::make_shared<PahoMqttClient>();
    auto connectivityService = std::make_shared<MqttConnectivityService>(mqttClient, m_device, m_host);

	auto inboundMessageHandler = std::make_shared<InboundMessageHandler>(m_device);
	auto outboundServiceDataHandler = std::make_shared<OutboundDataService>(m_device, connectivityService);

	auto serviceCommandHandler = std::make_shared<ServiceCommandHandler>();

	auto wolk = std::unique_ptr<Wolk>(new Wolk(connectivityService, m_persistence, serviceCommandHandler,
											   inboundMessageHandler, m_device));

    wolk->m_actuationHandlerLambda = m_actuationHandlerLambda;
    wolk->m_actuationHandler = m_actuationHandler;

    wolk->m_actuatorStatusProviderLambda = m_actuatorStatusProviderLambda;
    wolk->m_actuatorStatusProvider = m_actuatorStatusProvider;

	std::shared_ptr<FileDownloadMqttService> fileDownloadMqttService;
	std::shared_ptr<FileDownloadUrlService> fileDownloadUrlService;

	if(m_directFileDownloadEnabled)
	{
		fileDownloadMqttService = std::make_shared<FileDownloadMqttService>(outboundServiceDataHandler);
		serviceCommandHandler->setBinaryDataHandler(fileDownloadMqttService);
		wolk->addService(fileDownloadMqttService);
	}

	if(m_urlFileDownloader.lock() != nullptr)
	{
		fileDownloadUrlService = std::make_shared<FileDownloadUrlService>(outboundServiceDataHandler, m_urlFileDownloader);
		// TODO serviceCommandHandler->set
	}

	if(m_firmwareInstaller.lock() != nullptr)
	{
		auto firmwareUpdateService = std::make_shared<FirmwareUpdateService>(outboundServiceDataHandler, m_firmwareInstaller);
		serviceCommandHandler->setFirmwareUpdateCommandHandler(firmwareUpdateService);
		wolk->addService(firmwareUpdateService);

		std::weak_ptr<FirmwareUpdateService> firmwareUpdateService_weak{firmwareUpdateService};

		if(fileDownloadMqttService)
		{
			fileDownloadMqttService->setFileDownloadedCallback([=](const std::string& fileName) -> void {
				if(auto handler = firmwareUpdateService_weak.lock())
				{
					handler->onFirmwareFileDownloaded(fileName);
				}
			});
		}

		if(fileDownloadUrlService)
		{
			fileDownloadUrlService->setFileDownloadedCallback([=](const std::string& fileName) -> void {
				if(auto handler = firmwareUpdateService_weak.lock())
				{
					handler->onFirmwareFileDownloaded(fileName);
				}
			});
		}
	}

	inboundMessageHandler->setActuatorCommandHandler([&](const ActuatorCommand& actuatorCommand) -> void {
		wolk->handleActuatorCommand(actuatorCommand);
	});

	inboundMessageHandler->setBinaryDataHandler([&](const BinaryData& binaryData) -> void {
		serviceCommandHandler->handleBinaryData(binaryData);
	});

	inboundMessageHandler->setFileDownloadMqttCommandHandler([&](const FileDownloadMqttCommand& fileDownloadCommand) -> void {
		serviceCommandHandler->handleFileDownloadMqttCommand(fileDownloadCommand);
	});

	inboundMessageHandler->setFirmwareUpdateCommandHandler([&](const FirmwareUpdateCommand& firmwareUpdateCommand) -> void {
		serviceCommandHandler->handleFirmwareUpdateCommand(firmwareUpdateCommand);
	});

	connectivityService->setListener(std::dynamic_pointer_cast<ConnectivityServiceListener>(inboundMessageHandler));

    return wolk;
}

wolkabout::WolkBuilder::operator std::unique_ptr<Wolk>() const
{
	return build();
}

WolkBuilder::WolkBuilder(Device device)
	: m_host{WOLK_DEMO_HOST}, m_device{std::move(device)}, m_persistence{new InMemoryPersistence()}, m_directFileDownloadEnabled{false}
{
}
}

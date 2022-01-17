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
#include "core/protocol/wolkabout/WolkaboutErrorProtocol.h"
#include "core/protocol/wolkabout/WolkaboutFileManagementProtocol.h"
#include "core/protocol/wolkabout/WolkaboutFirmwareUpdateProtocol.h"
#include "core/protocol/wolkabout/WolkaboutPlatformStatusProtocol.h"
#include "core/protocol/wolkabout/WolkaboutRegistrationProtocol.h"
#include "core/utilities/Logger.h"
#include "wolk/InboundPlatformMessageHandler.h"
#include "wolk/WolkMulti.h"
#include "wolk/WolkSingle.h"
#include "wolk/connectivity/mqtt/WolkPahoMqttClient.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/file_management/FileManagementService.h"
#include "wolk/service/firmware_update/FirmwareUpdateService.h"

#include <stdexcept>
#include <utility>

namespace wolkabout
{
WolkBuilder::WolkBuilder(std::vector<Device> devices)
: m_devices(std::move(devices))
, m_host(WOLK_DEMO_HOST)
, m_caCertPath(TRUST_STORE)
, m_persistence{new InMemoryPersistence}
, m_dataProtocol{new WolkaboutDataProtocol}
, m_errorProtocol{new WolkaboutErrorProtocol}
, m_errorRetainTime{1000}
, m_fileTransferEnabled(false)
, m_fileTransferUrlEnabled(false)
, m_maxPacketSize{0}
{
}

WolkBuilder::WolkBuilder(Device device)
: m_devices{{std::move(device)}}
, m_host{WOLK_DEMO_HOST}
, m_caCertPath{TRUST_STORE}
, m_persistence{new InMemoryPersistence}
, m_dataProtocol{new WolkaboutDataProtocol}
, m_errorProtocol{new WolkaboutErrorProtocol}
, m_errorRetainTime{1000}
, m_fileTransferEnabled(false)
, m_fileTransferUrlEnabled(false)
, m_maxPacketSize{0}
{
}

std::vector<Device>& WolkBuilder::getDevices()
{
    return m_devices;
}

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

WolkBuilder& WolkBuilder::withPersistence(std::unique_ptr<Persistence> persistence)
{
    m_persistence = std::move(persistence);
    return *this;
}

WolkBuilder& WolkBuilder::withDataProtocol(std::unique_ptr<DataProtocol> protocol)
{
    m_dataProtocol = std::move(protocol);
    return *this;
}

WolkBuilder& WolkBuilder::withErrorProtocol(std::chrono::milliseconds errorRetainTime,
                                            std::unique_ptr<ErrorProtocol> protocol)
{
    m_errorRetainTime = errorRetainTime;
    if (protocol != nullptr)
        m_errorProtocol = std::move(protocol);
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

WolkBuilder& WolkBuilder::withFirmwareUpdate(std::unique_ptr<FirmwareInstaller> firmwareInstaller,
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

WolkBuilder& WolkBuilder::withFirmwareUpdate(std::unique_ptr<FirmwareParametersListener> firmwareParametersListener,
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

WolkBuilder& WolkBuilder::withPlatformStatus(std::unique_ptr<PlatformStatusListener> platformStatusListener)
{
    if (m_platformStatusProtocol == nullptr)
        m_platformStatusProtocol =
          std::unique_ptr<WolkaboutPlatformStatusProtocol>(new wolkabout::WolkaboutPlatformStatusProtocol);
    m_platformStatusListener = std::move(platformStatusListener);
    return *this;
}

WolkBuilder& WolkBuilder::withRegistration(std::unique_ptr<RegistrationProtocol> protocol)
{
    if (protocol == nullptr)
        m_registrationProtocol =
          std::unique_ptr<WolkaboutRegistrationProtocol>(new wolkabout::WolkaboutRegistrationProtocol);
    else
        m_registrationProtocol = std::move(protocol);
    return *this;
}

std::unique_ptr<WolkInterface> WolkBuilder::build(WolkInterfaceType type)
{
    LOG(TRACE) << METHOD_INFO;

    // Make the Wolk instance
    auto wolk = std::unique_ptr<WolkInterface>{};
    switch (type)
    {
    case WolkInterfaceType::SingleDevice:
    {
        wolk.reset(new WolkSingle{m_devices.front()});
        break;
    }
    case WolkInterfaceType::MultiDevice:
    {
        wolk.reset(new WolkMulti{m_devices});
        break;
    }
    }

    // Create the inbound message handler that will route all the messages by topic to their right destination
    auto deviceKeys = std::vector<std::string>{};
    for (const auto& device : m_devices)
        deviceKeys.emplace_back(device.getKey());
    wolk->m_inboundMessageHandler =
      std::unique_ptr<InboundMessageHandler>(new InboundPlatformMessageHandler(deviceKeys));

    // Now create the ConnectivityService.
    auto mqttClient = std::make_shared<WolkPahoMqttClient>();
    switch (type)
    {
    case WolkInterfaceType::SingleDevice:
    {
        const auto& device = m_devices.front();
        wolk->m_connectivityService = std::unique_ptr<MqttConnectivityService>(new MqttConnectivityService(
          mqttClient, device.getKey(), device.getPassword(), m_host, m_caCertPath,
          ByteUtils::toUUIDString(ByteUtils::generateRandomBytes(ByteUtils::UUID_VECTOR_SIZE))));
        break;
    }
    case WolkInterfaceType::MultiDevice:
    {
        wolk->m_connectivityService = std::unique_ptr<MqttConnectivityService>(new MqttConnectivityService(
          mqttClient, "", "", m_host, m_caCertPath,
          ByteUtils::toUUIDString(ByteUtils::generateRandomBytes(ByteUtils::UUID_VECTOR_SIZE))));
        break;
    }
    }

    // Connect the ConnectivityService with the ConnectivityManager.
    auto wolkRaw = wolk.get();
    wolk->m_connectivityManager = std::make_shared<WolkInterface::ConnectivityFacade>(*wolk->m_inboundMessageHandler,
                                                                                      [wolkRaw]
                                                                                      {
                                                                                          wolkRaw->notifyDisconnected();
                                                                                          wolkRaw->connect();
                                                                                      });
    wolk->m_connectivityService->setListener(wolk->m_connectivityManager);

    // Set the data service, the only required service
    wolk->m_dataProtocol = std::move(m_dataProtocol);
    wolk->m_errorProtocol = std::move(m_errorProtocol);
    wolk->m_persistence = std::move(m_persistence);
    wolk->m_feedUpdateHandlerLambda = m_feedUpdateHandlerLambda;
    wolk->m_feedUpdateHandler = m_feedUpdateHandler;
    wolk->m_parameterLambda = m_parameterHandlerLambda;
    wolk->m_parameterHandler = m_parameterHandler;
    wolk->m_dataService = std::make_shared<DataService>(
      *wolk->m_dataProtocol, *wolk->m_persistence, *wolk->m_connectivityService,
      [wolkRaw](const std::string& deviceKey, const std::map<std::uint64_t, std::vector<Reading>>& readings)
      { wolkRaw->handleFeedUpdateCommand(deviceKey, readings); },
      [wolkRaw](const std::string& deviceKey, const std::vector<Parameter>& parameters)
      { wolkRaw->handleParameterCommand(deviceKey, parameters); });
    wolk->m_errorService = std::make_shared<ErrorService>(*wolk->m_errorProtocol, m_errorRetainTime);
    wolk->m_inboundMessageHandler->addListener(wolk->m_dataService);
    wolk->m_inboundMessageHandler->addListener(wolk->m_errorService);
    wolk->m_errorService->start();

    // Check if the file management should be engaged
    if (m_fileManagementProtocol != nullptr)
    {
        // Create the File Management service
        wolk->m_fileManagementProtocol = std::move(m_fileManagementProtocol);
        wolk->m_fileManagementService = std::make_shared<FileManagementService>(
          *wolk->m_connectivityService, *wolk->m_dataService, *wolk->m_fileManagementProtocol, m_fileDownloadDirectory,
          m_fileTransferEnabled, m_fileTransferUrlEnabled, std::move(m_fileDownloader), std::move(m_fileListener));

        // Trigger the on build and add the listener for MQTT messages
        wolk->m_fileManagementService->createFolder();
        wolk->m_inboundMessageHandler->addListener(wolk->m_fileManagementService);
    }

    // Set the parameters about the FileTransfer
    for (const auto& device : m_devices)
    {
        wolk->m_dataService->updateParameter(
          device.getKey(), {ParameterName::FILE_TRANSFER_PLATFORM_ENABLED, m_fileTransferEnabled ? "true" : "false"});
        wolk->m_dataService->updateParameter(
          device.getKey(), {ParameterName::FILE_TRANSFER_URL_ENABLED, m_fileTransferUrlEnabled ? "true" : "false"});
    }

    // Check if the firmware update should be engaged
    if (m_firmwareUpdateProtocol != nullptr)
    {
        // Create the Firmware Update service
        wolk->m_firmwareUpdateProtocol = std::move(m_firmwareUpdateProtocol);

        // Build based on the module we have
        if (m_firmwareInstaller != nullptr)
        {
            wolk->m_firmwareUpdateService = std::make_shared<FirmwareUpdateService>(
              *wolk->m_connectivityService, *wolk->m_dataService, std::move(m_firmwareInstaller),
              *wolk->m_firmwareUpdateProtocol, m_workingDirectory);
        }
        else if (m_firmwareParametersListener != nullptr)
        {
            wolk->m_firmwareUpdateService = std::make_shared<FirmwareUpdateService>(
              *wolk->m_connectivityService, *wolk->m_dataService, std::move(m_firmwareParametersListener),
              *wolk->m_firmwareUpdateProtocol, m_workingDirectory);
        }

        // And set it all up
        for (const auto& device : m_devices)
            wolk->m_firmwareUpdateService->loadState(device.getKey());
        wolk->m_inboundMessageHandler->addListener(wolk->m_firmwareUpdateService);
    }

    // Set the parameters about the FirmwareUpdate
    for (const auto& device : m_devices)
    {
        wolk->m_dataService->updateParameter(
          device.getKey(),
          {ParameterName::FIRMWARE_UPDATE_ENABLED, wolk->m_firmwareUpdateProtocol != nullptr ? "true" : "false"});
        auto firmwareVersion = std::string{};
        if (m_firmwareInstaller != nullptr)
            firmwareVersion = m_firmwareInstaller->getFirmwareVersion(device.getKey());
        else if (m_firmwareParametersListener != nullptr)
            firmwareVersion = m_firmwareParametersListener->getFirmwareVersion();
        wolk->m_dataService->updateParameter(device.getKey(), {ParameterName::FIRMWARE_VERSION, firmwareVersion});
    }

    // Check if the platform status service needs to be introduced
    if (m_platformStatusProtocol != nullptr)
    {
        // Create the service
        wolk->m_platformStatusProtocol = std::move(m_platformStatusProtocol);
        wolk->m_platformStatusService =
          std::make_shared<PlatformStatusService>(*wolk->m_platformStatusProtocol, std::move(m_platformStatusListener));
        wolk->m_inboundMessageHandler->addListener(wolk->m_platformStatusService);
    }

    // Check if the registration service needs to be introduces
    if (m_registrationProtocol != nullptr)
    {
        // Create the service
        wolk->m_registrationProtocol = std::move(m_registrationProtocol);
        wolk->m_registrationService = std::make_shared<RegistrationService>(
          *wolk->m_registrationProtocol, *wolk->m_connectivityService, *wolk->m_errorService);
        wolk->m_inboundMessageHandler->addListener(wolk->m_registrationService);
    }

    return wolk;
}

std::unique_ptr<WolkSingle> WolkBuilder::buildWolkSingle()
{
    LOG(TRACE) << METHOD_INFO;

    // Check that the devices vector contains a single device.
    if (m_devices.size() != 1)
        throw std::runtime_error(
          "Failed to build `WolkSingle` instance: The devices vector does not contain exactly one device.");
    // Check that the device in the vector is valid
    const auto& device = m_devices.front();
    if (device.getKey().empty())
        throw std::runtime_error("Failed to build `WolkSingle` instance: The device contains an empty key.");
    if (device.getPassword().empty())
        throw std::runtime_error("Failed to build `WolkSingle` instance: The device contains an empty password.");

    // Cast the build pointer into the right type of unique_ptr.
    return std::unique_ptr<WolkSingle>(dynamic_cast<WolkSingle*>(build(WolkInterfaceType::SingleDevice).release()));
}

std::unique_ptr<WolkMulti> WolkBuilder::buildWolkMulti()
{
    LOG(TRACE) << METHOD_INFO;

    // Check that the devices vector is at least not empty.
    if (m_devices.empty())
        throw std::runtime_error("Failed to build `WolkMulti` instance: The devices vector is empty.");
    // Check that the devices don't contain empty keys.
    for (const auto& device : m_devices)
        if (device.getKey().empty())
            throw std::runtime_error(
              "Failed to build `WolkMulti` instance: One of the devices in the vector contains an empty key.");

    // Cast the build pointer into the right type of unique_ptr.
    return std::unique_ptr<WolkMulti>(dynamic_cast<WolkMulti*>(build(WolkInterfaceType::MultiDevice).release()));
}

wolkabout::WolkBuilder::operator std::unique_ptr<WolkInterface>()
{
    return build();
}
}    // namespace wolkabout

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

#ifndef WOLKBUILDER_H
#define WOLKBUILDER_H

#include "api/ActuationHandler.h"
#include "api/ActuatorStatusProvider.h"
#include "api/ConfigurationHandler.h"
#include "api/ConfigurationProvider.h"
#include "api/FirmwareInstaller.h"
#include "api/FirmwareVersionProvider.h"
#include "api/UrlFileDownloader.h"
#include "model/Device.h"
#include "persistence/Persistence.h"
#include "protocol/DataProtocol.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class Wolk;

class WolkBuilder final
{
public:
    ~WolkBuilder();

    WolkBuilder(WolkBuilder&&);

    /**
     * @brief WolkBuilder Initiates wolkabout::Wolk builder
     * @param device Device for which wolkabout::WolkBuilder is instantiated
     */
    WolkBuilder(Device device);

    /**
     * @brief Allows passing of URI to custom WolkAbout IoT platform instance
     * @param host Server URI
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& host(const std::string& host);

    /**
     * @brief Allows passing of custom CA’s public certificate file to custom WolkAbout IoT platform instance
     * @param ca_cert_path ca.crt file system path
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& ca_cert_path(const std::string& ca_cert_path);


    /**
     * @brief Sets actuation handler
     * @param actuationHandler Lambda that handles actuation requests
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& actuationHandler(
      const std::function<void(const std::string& reference, const std::string& value)>& actuationHandler);

    /**
     * @brief Sets actuation handler
     * @param actuationHandler Instance of wolkabout::ActuationHandler
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& actuationHandler(std::weak_ptr<ActuationHandler> actuationHandler);

    /**
     * @brief Sets actuation status provider
     * @param actuatorStatusProvider Lambda that provides ActuatorStatus by reference of requested actuator
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& actuatorStatusProvider(
      const std::function<ActuatorStatus(const std::string& reference)>& actuatorStatusProvider);

    /**
     * @brief Sets actuation status provider
     * @param actuatorStatusProvider Instance of wolkabout::ActuatorStatusProvider
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& actuatorStatusProvider(std::weak_ptr<ActuatorStatusProvider> actuatorStatusProvider);

    /**
     * @brief Sets device configuration handler
     * @param configurationHandler Lambda that handles setting of configuration
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& configurationHandler(
      std::function<void(const std::vector<ConfigurationItem>& configuration)> configurationHandler);

    /**
     * @brief Sets device configuration handler
     * @param configurationHandler Instance of wolkabout::ConfigurationHandler that handles setting of configuration
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& configurationHandler(std::weak_ptr<ConfigurationHandler> configurationHandler);

    /**
     * @brief Sets device configuration provider
     * @param configurationProvider Lambda that provides device configuration
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& configurationProvider(std::function<std::vector<ConfigurationItem>()> configurationProvider);

    /**
     * @brief Sets device configuration provider
     * @param configurationProvider Instance of wolkabout::ConfigurationProvider that provides device configuration
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& configurationProvider(std::weak_ptr<ConfigurationProvider> configurationProvider);

    /**
     * @brief Sets underlying persistence mechanism to be used<br>
     *        Sample in-memory persistence is used as default
     * @param persistence std::shared_ptr to wolkabout::Persistence implementation
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withPersistence(std::shared_ptr<Persistence> persistence);

    /**
     * @brief withDataProtocol Defines which data protocol to use
     * @param Protocol unique_ptr to wolkabout::DataProtocol implementation
     * @return Reference to current wolkabout::WolkBuilder instance (Provides
     * fluent interface)
     */
    WolkBuilder& withDataProtocol(std::unique_ptr<DataProtocol> protocol);

    /**
     * @brief withFileManagement enables file transfer with the platform
     * @param fileDownloadDirectory path to folder where to store files
     * @param maxPacketSize prefered file packet size in bytes
     * @return Reference to current wolkabout::WolkBuilder instance (Provides
     * fluent interface)
     */
    WolkBuilder& withFileManagement(const std::string& fileDownloadDirectory, std::uint64_t maxPacketSize);

    /**
     * @brief withFileManagement enables file transfer with the platform
     * @param fileDownloadDirectory path to folder where to store files
     * @param maxPacketSize prefered file packet size in bytes
     * @param urlDownloader Instance of wolkabout::UrlFileDownloader
     * @return Reference to current wolkabout::WolkBuilder instance (Provides
     * fluent interface)
     */
    WolkBuilder& withFileManagement(const std::string& fileDownloadDirectory, std::uint64_t maxPacketSize,
                                    std::shared_ptr<UrlFileDownloader> urlDownloader);

    /**
     * @brief withFirmwareUpdate Enables firmware update for device, requires file management
     * @param installer Instance of wolkabout::FirmwareInstaller used to install firmware
     * @param provider Instance of wolkabout::FirmwareVersionProvider used to provide
     * firmware version of the device
     * @return
     */
    WolkBuilder& withFirmwareUpdate(std::shared_ptr<FirmwareInstaller> installer,
                                    std::shared_ptr<FirmwareVersionProvider> provider);

    /**
     * @brief withoutKeepAlive Disables ping mechanism used to notify WolkAbout IOT Platform
     * that device is still connected
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withoutKeepAlive();

    /**
     * @brief Builds Wolk instance
     * @return Wolk instance as std::unique_ptr<Wolk>
     *
     * @throws std::logic_error if device key is not present in wolkabout::Device
     * @throws std::logic_error if actuator status provider is not set, and wolkabout::Device has actuator references
     * @throws std::logic_error if actuation handler is not set, and wolkabout::Device has actuator references
     */
    std::unique_ptr<Wolk> build();

    /**
     * @brief operator std::unique_ptr<Wolk> Conversion to wolkabout::wolk as result returns std::unique_ptr to built
     * wolkabout::Wolk instance
     */
    operator std::unique_ptr<Wolk>();

private:
    std::string m_host;
    std::string m_ca_cert_path;
    Device m_device;

    std::function<void(std::string, std::string)> m_actuationHandlerLambda;
    std::weak_ptr<ActuationHandler> m_actuationHandler;

    std::function<ActuatorStatus(std::string)> m_actuatorStatusProviderLambda;
    std::weak_ptr<ActuatorStatusProvider> m_actuatorStatusProvider;

    std::function<void(const std::vector<ConfigurationItem>& configuration)> m_configurationHandlerLambda;
    std::weak_ptr<ConfigurationHandler> m_configurationHandler;

    std::function<std::vector<ConfigurationItem>()> m_configurationProviderLambda;
    std::weak_ptr<ConfigurationProvider> m_configurationProvider;

    std::shared_ptr<Persistence> m_persistence;
    std::unique_ptr<DataProtocol> m_dataProtocol;

    std::string m_firmwareVersion;
    std::string m_fileDownloadDirectory;
    std::uint64_t m_maxPacketSize;
    std::shared_ptr<FirmwareInstaller> m_firmwareInstaller;
    std::shared_ptr<FirmwareVersionProvider> m_firmwareVersionProvider;
    std::shared_ptr<UrlFileDownloader> m_urlFileDownloader = nullptr;

    bool m_fileManagementEnabled = false;

    bool m_keepAliveEnabled;

    static const constexpr char* WOLK_DEMO_HOST = "ssl://api-demo.wolkabout.com:8883";
    static const constexpr char* TRUST_STORE = "ca.crt";
    static const constexpr char* DATABASE = "fileRepository.db";
};
}    // namespace wolkabout

#endif

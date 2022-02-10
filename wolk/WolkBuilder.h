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

#ifndef WOLKBUILDER_H
#define WOLKBUILDER_H

#include "core/model/Device.h"
#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/protocol/ErrorProtocol.h"
#include "core/protocol/FileManagementProtocol.h"
#include "core/protocol/FirmwareUpdateProtocol.h"
#include "core/protocol/PlatformStatusProtocol.h"
#include "core/protocol/RegistrationProtocol.h"
#include "wolk/WolkInterfaceType.h"
#include "wolk/api/FeedUpdateHandler.h"
#include "wolk/api/FileListener.h"
#include "wolk/api/FirmwareInstaller.h"
#include "wolk/api/FirmwareParametersListener.h"
#include "wolk/api/ParameterHandler.h"
#include "wolk/api/PlatformStatusListener.h"
#include "wolk/service/file_management/FileDownloader.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
namespace connect
{
// Forward declare all the Wolk classes that can be outputs from this builder.
class WolkInterface;
class WolkMulti;
class WolkSingle;

/**
 * This is the class that is meant to bring in all the parameters, and create an instance of Wolk.
 */
class WolkBuilder final
{
public:
    /**
     * @brief WolkBuilder move constructors.
     */
    WolkBuilder(WolkBuilder&&) noexcept = default;

    /**
     * @brief Initiates the builder.
     * @param devices The list of devices for the Wolk instance. Restricts from building WolkSingle when there is not
     * exactly a single device in the list.
     */
    explicit WolkBuilder(std::vector<Device> devices = {});

    /**
     * @brief WolkBuilder Initiates wolkabout::Wolk builder
     * @param device Device for which wolkabout::WolkBuilder is instantiated
     */
    explicit WolkBuilder(Device device);

    /**
     * @brief Returns the reference to the vector that holds the device, allowing the user to manipulate the vector.
     * @return The reference to the device vector.
     */
    std::vector<Device>& getDevices();

    /**
     * @brief Allows passing of URI to custom WolkAbout IoT platform instance
     * @param host Server URI
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& host(const std::string& host);

    /**
     * @brief Allows passing of custom CA’s public certificate file to custom WolkAbout IoT platform instance
     * @param caCertPath ca.crt file system path
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& caCertPath(const std::string& caCertPath);

    /**
     * @brief Sets feed update handler
     * @param feedUpdateHandler Lambda that handles feed update requests. Will receive a map of readings grouped by the
     * time when the update happened. Key is a timestamp in milliseconds, and value is a vector of readings that changed
     * at that time.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& feedUpdateHandler(
      const std::function<void(std::string, const std::map<std::uint64_t, std::vector<Reading>>)>& feedUpdateHandler);

    /**
     * @brief Sets feed update handler
     * @param feedUpdateHandler Instance of wolkabout::FeedUpdateHandler
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& feedUpdateHandler(std::weak_ptr<FeedUpdateHandler> feedUpdateHandler);

    /**
     * @brief Sets parameter handler
     * @param parameterHandlerLambda Lambda that handles parameters updates
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& parameterHandler(
      const std::function<void(std::string, std::vector<Parameter>)>& parameterHandlerLambda);

    /**
     * @brief Sets parameter handler
     * @param parameterHandler Instance of wolkabout::ParameterHandler
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& parameterHandler(std::weak_ptr<ParameterHandler> parameterHandler);

    /**
     * @brief Sets underlying persistence mechanism to be used<br>
     *        Sample in-memory persistence is used as default
     * @param persistence std::shared_ptr to wolkabout::Persistence implementation
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withPersistence(std::unique_ptr<Persistence> persistence);

    /**
     * @brief withDataProtocol Defines which data protocol to use
     * @param Protocol unique_ptr to wolkabout::DataProtocol implementation
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withDataProtocol(std::unique_ptr<DataProtocol> protocol);

    /**
     * @brief withErrorProtocol Defines which error protocol to use
     * @param errorRetainTime The time defining how long will the error messages be retained. The default retain time is
     * 1s (1000ms).
     * @param protocol Unique_ptr to wolkabout::ErrorProtocol implementation (providing nullptr will still refer to
     * default one)
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withErrorProtocol(std::chrono::milliseconds errorRetainTime,
                                   std::unique_ptr<ErrorProtocol> protocol = nullptr);

    /**
     * @brief Sets the Wolk module to allow file management functionality.
     * @details This one is meant to enable the File Transfer, but not File URL Download.
     * @param fileDownloadLocation The folder location for file management.
     * @param maxPacketSize The maximum packet size for downloading chunks (in KBs).
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFileTransfer(const std::string& fileDownloadLocation, std::uint64_t maxPacketSize = 268435);

    /**
     * @brief Sets the Wolk module to allow file management functionality.
     * @details This one is meant to enable File URL Download, but can enabled File Transfer too.
     * @param fileDownloadLocation The folder location for file management.
     * @param fileDownloader The implementation that will download the files.
     * @param transferEnabled Whether the File Transfer should be enabled too.
     * @param maxPacketSize The max packet size for downloading chunks (in MBs).
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFileURLDownload(const std::string& fileDownloadLocation,
                                     std::shared_ptr<FileDownloader> fileDownloader = nullptr,
                                     bool transferEnabled = false, std::uint64_t maxPacketSize = 268435);

    /**
     * @brief Sets the Wolk module file listener.
     * @details This object will receive information about newly obtained or removed files. It will be used with
     * `withFileListener` when the service gets created.
     * @param fileListener A pointer to the instance of the file listener.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFileListener(const std::shared_ptr<FileListener>& fileListener);

    /**
     * @brief Sets the Wolk module to allow firmware update functionality.
     * @details This one is meant for PUSH configuration, where the functionality is implemented using the
     * `FirmwareInstaller`. This object will received instructions from the platform of when to install new firmware.
     * @param firmwareInstaller The implementation of the FirmwareInstaller interface.
     * @param workingDirectory The directory where the session file will be kept.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFirmwareUpdate(std::unique_ptr<FirmwareInstaller> firmwareInstaller,
                                    const std::string& workingDirectory = "./");

    /**
     * @brief Sets the Wolk module to allow firmware update functionality.
     * @details This one is meant for PULL configuration, where the functionality is implemented using the
     * `FirmwareParametersListener`. This object will receive information from the platform of when and where to check
     * for firmware updates.
     * @param firmwareParametersListener The implementation of the FirmwareParametersListener interface.
     * @param workingDirectory The directory where the session file will be kept.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFirmwareUpdate(std::unique_ptr<FirmwareParametersListener> firmwareParametersListener,
                                    const std::string& workingDirectory = "./");

    /**
     * @brief Sets the Wolk module to allow listening to `p2d/platform_status` messages.
     * @param platformStatusListener The listener of the messages.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withPlatformStatus(std::shared_ptr<PlatformStatusListener> platformStatusListener);

    /**
     * @brief Sets the Wolk module to allow device registration.
     * @param protocol The protocol that will be used for registration. If remained as nullptr, default one will be
     * used.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withRegistration(std::unique_ptr<RegistrationProtocol> protocol = nullptr);

    /**
     * @brief Builds a WolkInterface instance.
     * @param type The type of the WolkInterface that the builder should build.
     * @return Wolk instance as std::unique_ptr<WolkInterface>. This should be cast.
     */
    std::unique_ptr<WolkInterface> build(WolkInterfaceType type = WolkInterfaceType::SingleDevice);

    /**
     * @brief Builds WolkSingle instance.
     * @return WolkSingle instance as std::unique_ptr<WolkSingle>.
     *
     * @throws std::runtime_error If the devices vector does not contain exactly one device.
     * @throws std::runtime_error If the device in the vector contains an empty key or password.
     */
    std::unique_ptr<WolkSingle> buildWolkSingle();

    /**
     * @brief Builds WolkMulti instance.
     * @return WolkMulti instance as std::unique_ptr<WolkMulti>.
     */
    std::unique_ptr<WolkMulti> buildWolkMulti();

private:
    // Here we store the list of devices that the Wolk instance will handle
    std::vector<Device> m_devices;

    // Here is the place for all the connectivity parameters
    std::string m_host;
    std::string m_caCertPath;

    // Here is the place for external entities capable of receiving Reading values.
    std::function<void(std::string, std::map<std::uint64_t, std::vector<Reading>>)> m_feedUpdateHandlerLambda;
    std::weak_ptr<FeedUpdateHandler> m_feedUpdateHandler;

    // Here is the place for external entities capable of receiving Parameter values.
    std::function<void(std::string, std::vector<Parameter>)> m_parameterHandlerLambda;
    std::weak_ptr<ParameterHandler> m_parameterHandler;

    // Here is the place for the persistence pointer
    std::unique_ptr<Persistence> m_persistence;

    // Here is the place for all the protocols that are being held
    std::unique_ptr<DataProtocol> m_dataProtocol;
    std::unique_ptr<ErrorProtocol> m_errorProtocol;
    std::chrono::milliseconds m_errorRetainTime;
    std::unique_ptr<FileManagementProtocol> m_fileManagementProtocol;
    std::unique_ptr<FirmwareUpdateProtocol> m_firmwareUpdateProtocol;
    std::unique_ptr<PlatformStatusProtocol> m_platformStatusProtocol;
    std::unique_ptr<RegistrationProtocol> m_registrationProtocol;

    // Here is the place for all the file transfer related parameters
    std::shared_ptr<FileDownloader> m_fileDownloader;
    std::string m_fileDownloadDirectory;
    bool m_fileTransferEnabled;
    bool m_fileTransferUrlEnabled;
    std::uint64_t m_maxPacketSize;
    std::shared_ptr<FileListener> m_fileListener;

    // Here is the place for all the firmware update related parameters
    std::unique_ptr<FirmwareInstaller> m_firmwareInstaller;
    std::string m_workingDirectory;
    std::unique_ptr<FirmwareParametersListener> m_firmwareParametersListener;

    // Here is the place for the platform status listener
    std::shared_ptr<PlatformStatusListener> m_platformStatusListener;

    // These are the default values that are going to be used for the connection parameters
    static const constexpr char* WOLK_DEMO_HOST = "ssl://api-demo.wolkabout.com:8883";
    static const constexpr char* TRUST_STORE = "ca.crt";
};
}    // namespace connect
}    // namespace wolkabout

#endif

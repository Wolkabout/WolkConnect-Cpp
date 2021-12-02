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
#include "core/protocol/FileManagementProtocol.h"
#include "core/protocol/FirmwareUpdateProtocol.h"
#include "wolk/api/FeedUpdateHandler.h"
#include "wolk/api/FileListener.h"
#include "wolk/api/FirmwareInstaller.h"
#include "wolk/api/FirmwareParametersListener.h"
#include "wolk/service/file_management/poco/HTTPFileDownloader.h"

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
    /**
     * @brief WolkBuilder move constructors.
     */
    WolkBuilder(WolkBuilder&&) noexcept = default;

    /**
     * @brief WolkBuilder Initiates wolkabout::Wolk builder
     * @param device Device for which wolkabout::WolkBuilder is instantiated
     */
    explicit WolkBuilder(Device device);

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
     * @brief Sets feed update handler
     * @param feedUpdateHandler Lambda that handles feed update requests
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& feedUpdateHandler(
      const std::function<void(const std::map<std::uint64_t, std::vector<Reading>>)>& feedUpdateHandler);

    /**
     * @brief Sets feed update handler
     * @param feedUpdateHandler Instance of wolkabout::FeedUpdateHandler
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& feedUpdateHandler(std::weak_ptr<FeedUpdateHandler> feedUpdateHandler);

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
     * @brief Sets the Wolk module to allow file management functionality.
     * @details This one is meant to enable the File Transfer, but not File URL Download.
     * @param fileDownloadLocation The folder location for file management.
     * @param maxPacketSize The maximum packet size for downloading chunks (in MBs).
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFileTransfer(const std::string& fileDownloadLocation, std::uint64_t maxPacketSize = 268);

    /**
     * @brief Sets the Wolk module to allow file management functionality.
     * @details This one is meant to enable File URL Download, but can enabled File Transfer too.
     * @param fileDownloadLocation The folder location for file management.
     * @param fileDownloader The implementation that will download the files. By default our Poco HTTPFileDownloader.
     * @param transferEnabled Whether the File Transfer should be enabled too.
     * @param maxPacketSize The max packet size for downloading chunks (in MBs).
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFileURLDownload(const std::string& fileDownloadLocation,
                                     std::shared_ptr<FileDownloader> fileDownloader = nullptr,
                                     bool transferEnabled = false, std::uint64_t maxPacketSize = 268);

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
    WolkBuilder& withFirmwareUpdate(std::shared_ptr<FirmwareInstaller> firmwareInstaller,
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
    WolkBuilder& withFirmwareUpdate(std::shared_ptr<FirmwareParametersListener> firmwareParametersListener,
                                    const std::string& workingDirectory = "./");

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
    explicit operator std::unique_ptr<Wolk>();

private:
    std::string m_host;
    std::string m_ca_cert_path;
    Device m_device;

    std::function<void(std::map<std::uint64_t, std::vector<Reading>>)> m_feedUpdateHandlerLambda;
    std::weak_ptr<FeedUpdateHandler> m_feedUpdateHandler;

    std::shared_ptr<Persistence> m_persistence;
    std::unique_ptr<DataProtocol> m_dataProtocol;
    std::unique_ptr<FileManagementProtocol> m_fileManagementProtocol;
    std::unique_ptr<FirmwareUpdateProtocol> m_firmwareUpdateProtocol;

    std::shared_ptr<FileDownloader> m_fileDownloader;
    std::string m_fileDownloadDirectory;
    bool m_fileTransferEnabled;
    bool m_fileTransferUrlEnabled;
    std::uint64_t m_maxPacketSize;

    std::shared_ptr<FileListener> m_fileListener;

    std::shared_ptr<FirmwareInstaller> m_firmwareInstaller;
    std::string m_workingDirectory;
    std::shared_ptr<FirmwareParametersListener> m_firmwareParametersListener;

    static const constexpr char* WOLK_DEMO_HOST = "ssl://api-demo.wolkabout.com:8883";
    static const constexpr char* TRUST_STORE = "ca.crt";
};
}    // namespace wolkabout

#endif

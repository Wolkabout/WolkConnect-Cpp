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

#include "api/FeedUpdateHandler.h"
#include "core/model/Device.h"
#include "core/persistence/Persistence.h"
#include "core/protocol/DataProtocol.h"
#include "core/protocol/FileManagementProtocol.h"

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
     * @param fileDownloadLocation The folder location for file management.
     * @param fileTransferEnabled Whether the regular platform file transfer should be enabled.
     * @param fileTransferUrlEnabled Whether the url transfer should be enabled.
     * @param maxPacketSize The maximum packet size for downloading chunks.
     * @return Reference to current wolkabout::WolkBuilder instance (Provides fluent interface)
     */
    WolkBuilder& withFileManagement(const std::string& fileDownloadLocation, bool fileTransferEnabled = true,
                                    bool fileTransferUrlEnabled = true, std::uint64_t maxPacketSize = 268435455);

    WolkBuilder& withFirmwareUpdate();

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

    std::function<void(std::map<std::uint64_t, std::vector<Reading>>)> m_feedUpdateHandlerLambda;
    std::weak_ptr<FeedUpdateHandler> m_feedUpdateHandler;

    std::shared_ptr<Persistence> m_persistence;
    std::unique_ptr<DataProtocol> m_dataProtocol;
    std::unique_ptr<FileManagementProtocol> m_fileManagementProtocol;

    std::string m_fileDownloadDirectory;
    bool m_fileTransferEnabled;
    bool m_fileTransferUrlEnabled;
    std::uint64_t m_maxPacketSize;
    std::string m_firmwareVersion;

    static const constexpr char* WOLK_DEMO_HOST = "ssl://api-demo.wolkabout.com:8883";
    static const constexpr char* TRUST_STORE = "ca.crt";
    static const constexpr char* DATABASE = "fileRepository.db";
};
}    // namespace wolkabout

#endif

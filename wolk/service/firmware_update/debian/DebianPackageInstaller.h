/**
 * Copyright 2022 Wolkabout Technology s.r.o.
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

#ifndef WOLKABOUTCONNECTOR_DEBIANPACKAGEINSTALLER_H
#define WOLKABOUTCONNECTOR_DEBIANPACKAGEINSTALLER_H

#include "core/utility/Service.h"
#include "wolk/api/FirmwareInstaller.h"
#include "wolk/service/firmware_update/debian/apt/APTPackageInstaller.h"
#include "wolk/service/firmware_update/debian/systemd/SystemdServiceInterface.h"

namespace wolkabout::connect
{
// This is the type alias for the installation callback which will report the success of updating with one specific
// file.
using UpdateCallback = std::function<void(const std::string&, bool)>;

/**
 * This is the class that implements the entire Debian package update mechanism.
 */
class DebianPackageInstaller : public FirmwareInstaller, public legacy::Service
{
public:
    /**
     * Default constructor. Takes in the implementations of two services.
     *
     * @param serviceName The name of the service being managed.
     * @param aptPackageInstaller Pointer to an APTPackageInstaller.
     * @param systemdServiceInterface Pointer to a SystemdServiceInterface.
     */
    DebianPackageInstaller(std::string serviceName, std::unique_ptr<APTPackageInstaller> aptPackageInstaller,
                           std::unique_ptr<SystemdServiceInterface> systemdServiceInterface);

    /**
     * Default move constructor.
     *
     * @param instance Past instance of the installation.
     */
    DebianPackageInstaller(DebianPackageInstaller&& instance) noexcept;

    /**
     * Default overridden destructor. Will stop the underlying services.
     */
    ~DebianPackageInstaller() override;

    /**
     * The overridden method from the `wolkabout::Service` interface.
     * Will start the underlying services.
     */
    void start() override;

    /**
     * The overridden method from the `wolkabout::Service` interface.
     * Will stop the underlying services.
     */
    void stop() override;

    /**
     * This is the method that is used to update the package and restart it.
     * The service that will be restarted is the first part of the name of the debian file.
     *
     * @param pathToDebianFile The path to the file that needs to be installed.
     * @param callback The callback which will be called once the package is successfully installed and the service
     * restarted.
     * @return Whether the update process has been successfully started. If not, the callback will never be called.
     */
    virtual bool update(const std::string& pathToDebianFile, UpdateCallback callback);

    /**
     * This is the overridden method from the `wolkabout::connect::FirmwareInstaller` interface.
     * This is the method that is invoked once an installation is triggered by the platform.
     *
     * @param deviceKey The device for which the installation is supposed to happen.
     * @param fileName The name/path for the file that is supposed to be installed.
     * @return The response status for the installation of the file.
     */
    InstallResponse installFirmware(const std::string& deviceKey, const std::string& fileName) override;

    /**
     * This is the overridden method from the `wolkabout::connect::FirmwareInstaller` interface.
     * This is the method that is invoked once an abort is triggered by the platform.
     *
     * @param deviceKey The device for which the abort is sent.
     */
    void abortFirmwareInstall(const std::string& deviceKey) override;

    /**
     * This is the overridden method from the `wolkabout::connect::FirmwareInstaller` interface.
     * This is the method that is used to check the firmware update status.
     *
     * @param deviceKey The device for which the version is sent.
     * @param oldVersion The old version of software that was installed.
     * @return Whether the install was successful.
     */
    bool wasFirmwareInstallSuccessful(const std::string& deviceKey, const std::string& oldVersion) override;

    /**
     * This is the overridden method from the `wolkabout::connect::FirmwareInstaller` interface.
     * This is the method that can return the current firmware version.
     *
     * @param deviceKey The device for which the version is queried.
     * @return The device firmware version.
     */
    std::string getFirmwareVersion(const std::string& deviceKey) override;

protected:
    // The logger tag
    const std::string TAG = "[DebianPackageInstallation] -> ";

    // Name of the service
    const std::string m_serviceName;

    // Here we store the modules that accomplish the functionalities
    std::unique_ptr<APTPackageInstaller> m_aptPackageInstaller;
    std::unique_ptr<SystemdServiceInterface> m_systemdServiceInterface;

    // Here we store the callbacks
    std::unordered_map<std::string, UpdateCallback> m_callbacks;

    // And the command buffer where to execute the callbacks
    legacy::CommandBuffer m_commandBuffer;
    std::mutex m_mapMutex;
    std::map<std::string, std::string> m_devicesInstallingFiles;
    std::map<std::string, InstallResponse> m_deviceInstallationResult;
    std::map<std::string, std::shared_ptr<std::condition_variable>> m_conditionVariables;
};
}    // namespace wolkabout::connect

#endif    // WOLKABOUTCONNECTOR_DEBIANPACKAGEINSTALLER_H

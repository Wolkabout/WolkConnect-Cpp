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

#ifndef WOLKABOUTCONNECTOR_DEBIANPACKAGEINSTALLATION_H
#define WOLKABOUTCONNECTOR_DEBIANPACKAGEINSTALLATION_H

#include "core/utilities/Service.h"
#include "wolk/api/FirmwareInstaller.h"
#include "wolk/service/firmware_update/debian/apt/APTPackageInstaller.h"
#include "wolk/service/firmware_update/debian/systemd/SystemdServiceInterface.h"

namespace wolkabout
{
namespace connect
{
// This is the type alias for the installation callback which will report the success of updating with one specific
// file.
using UpdateCallback = std::function<void(const std::string&, bool)>;

/**
 * This is the class that implements the entire Debian package update mechanism.
 */
class DebianPackageInstallation : public FirmwareInstaller, public Service
{
public:
    /**
     * Default constructor. Takes in the implementations of two services.
     *
     * @param aptPackageInstaller Pointer to an APTPackageInstaller.
     * @param systemdServiceInterface Pointer to a SystemdServiceInterface.
     */
    DebianPackageInstallation(std::unique_ptr<APTPackageInstaller> aptPackageInstaller,
                              std::unique_ptr<SystemdServiceInterface> systemdServiceInterface);

    /**
     * Default overridden destructor. Will stop the underlying services.
     */
    ~DebianPackageInstallation() override;

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

    InstallResponse installFirmware(const std::string& deviceKey, const std::string& fileName) override;
    void abortFirmwareInstall(const std::string& deviceKey) override;
    bool wasFirmwareInstallSuccessful(const std::string& deviceKey, const std::string& oldVersion) override;
    std::string getFirmwareVersion(const std::string& deviceKey) override;

protected:
    // The logger tag
    const std::string TAG = "[DebianPackageInstallation] -> ";

    // Here we store the modules that accomplish the functionalities
    std::unique_ptr<APTPackageInstaller> m_aptPackageInstaller;
    std::unique_ptr<SystemdServiceInterface> m_systemdServiceInterface;

    // Here we store the callbacks
    std::unordered_map<std::string, UpdateCallback> m_callbacks;

    // And the command buffer where to execute the callbacks
    CommandBuffer m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_DEBIANPACKAGEINSTALLATION_H

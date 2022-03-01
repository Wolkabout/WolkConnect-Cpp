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

#include "wolk/service/firmware_update/debian/DebianPackageInstallation.h"

#include "core/utilities/Logger.h"

namespace wolkabout
{
namespace connect
{
DebianPackageInstallation::DebianPackageInstallation(std::unique_ptr<APTPackageInstaller> aptPackageInstaller,
                                                     std::unique_ptr<SystemdServiceInterface> systemdServiceInterface)
: m_aptPackageInstaller(std::move(aptPackageInstaller)), m_systemdServiceInterface(std::move(systemdServiceInterface))
{
}

DebianPackageInstallation::~DebianPackageInstallation()
{
    stop();
}

void DebianPackageInstallation::start()
{
    m_aptPackageInstaller->start();
    m_systemdServiceInterface->start();
}

void DebianPackageInstallation::stop()
{
    m_aptPackageInstaller->stop();
    m_systemdServiceInterface->stop();
}

bool DebianPackageInstallation::update(const std::string& pathToDebianFile, UpdateCallback callback)
{
    LOG(TRACE) << METHOD_INFO;

    // Define the callback that will be used once the APT package has been installed
    const auto serviceName = pathToDebianFile.substr(0, pathToDebianFile.find('_'));
    const auto internalCallback = InstallationCallback{[=](const std::string&, InstallationResult result) {
        if (result == InstallationResult::Installed)
            callback(pathToDebianFile,
                     m_systemdServiceInterface->restartService(serviceName) == ServiceRestartResult::Successful);
        callback(pathToDebianFile, false);
    }};

    // Attempt to install the package with APT
    return m_aptPackageInstaller->installPackage(pathToDebianFile, internalCallback) == InstallationResult::Installing;
}

InstallResponse DebianPackageInstallation::installFirmware(const std::string& deviceKey, const std::string& fileName)
{
    // TODO
    return InstallResponse::NO_FILE;
}

void DebianPackageInstallation::abortFirmwareInstall(const std::string& deviceKey)
{
    // TODO
}

bool DebianPackageInstallation::wasFirmwareInstallSuccessful(const std::string& deviceKey,
                                                             const std::string& oldVersion)
{
    // TODO
    return FirmwareInstaller::wasFirmwareInstallSuccessful(deviceKey, oldVersion);
}

std::string DebianPackageInstallation::getFirmwareVersion(const std::string& deviceKey)
{
    // TODO
    return std::string();
}
}    // namespace connect
}    // namespace wolkabout

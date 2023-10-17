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

#include "wolk/service/firmware_update/debian/DebianPackageInstaller.h"

#include "core/utility/FileSystemUtils.h"
#include "core/utility/Logger.h"

#include <utility>

using namespace wolkabout::legacy;

namespace wolkabout::connect
{
DebianPackageInstaller::DebianPackageInstaller(std::string serviceName,
                                               std::unique_ptr<APTPackageInstaller> aptPackageInstaller,
                                               std::unique_ptr<SystemdServiceInterface> systemdServiceInterface)
: m_serviceName{std::move(serviceName)}
, m_aptPackageInstaller{std::move(aptPackageInstaller)}
, m_systemdServiceInterface{std::move(systemdServiceInterface)}
{
}

DebianPackageInstaller::DebianPackageInstaller(DebianPackageInstaller&& instance) noexcept
: m_serviceName{instance.m_serviceName}
, m_aptPackageInstaller{std::move(instance.m_aptPackageInstaller)}
, m_systemdServiceInterface{std::move(instance.m_systemdServiceInterface)}
{
}

DebianPackageInstaller::~DebianPackageInstaller()
{
    std::lock_guard<std::mutex> lockGuard{m_mapMutex};
    for (const auto& pair : m_deviceInstallationResult)
    {
        if (pair.second != InstallResponse::WILL_INSTALL)
        {
            const auto fileIt = m_devicesInstallingFiles.find(pair.first);
            if (fileIt != m_devicesInstallingFiles.cend())
            {
                const auto conditionVariableIt = m_conditionVariables.find(fileIt->second);
                if (conditionVariableIt != m_conditionVariables.cend())
                {
                    conditionVariableIt->second->notify_one();
                }
            }
        }
    }
    stop();
}

void DebianPackageInstaller::start()
{
    m_aptPackageInstaller->start();
    m_systemdServiceInterface->start();
}

void DebianPackageInstaller::stop()
{
    m_aptPackageInstaller->stop();
    m_systemdServiceInterface->stop();
}

bool DebianPackageInstaller::update(const std::string& pathToDebianFile, UpdateCallback callback)
{
    LOG(TRACE) << METHOD_INFO;

    // Define the callback that will be used once the APT package has been installed
    auto serviceName = pathToDebianFile.substr(pathToDebianFile.rfind('/') + 1);
    serviceName = serviceName.substr(0, serviceName.find('_'));
    const auto internalCallback = InstallationCallback{[=](const std::string&, InstallationResult result) {
        if (result == InstallationResult::Installed)
        {
            callback(pathToDebianFile,
                     m_systemdServiceInterface->restartService(serviceName) == ServiceRestartResult::Successful);
            return;
        }
        callback(pathToDebianFile, false);
    }};

    // Attempt to install the package with APT
    return m_aptPackageInstaller->installPackage(pathToDebianFile, internalCallback) == InstallationResult::Installing;
}

InstallResponse DebianPackageInstaller::installFirmware(const std::string& deviceKey, const std::string& fileName)
{
    // We should do this synchronously. Prepare things to be set.
    auto conditionVariable = std::make_shared<std::condition_variable>();
    auto mutex = std::make_shared<std::mutex>();
    {
        std::lock_guard<std::mutex> lockGuard{m_mapMutex};
        m_devicesInstallingFiles[deviceKey] = fileName;
        m_deviceInstallationResult[deviceKey] = InstallResponse::WILL_INSTALL;
        m_conditionVariables[fileName] = conditionVariable;
    }

    // Run the update
    const auto mutexWeakPtr = std::weak_ptr<std::mutex>{mutex};
    auto installing = update(FileSystemUtils::absolutePath(fileName), [this, deviceKey, fileName, mutexWeakPtr](
                                                                        const std::string&, bool installation) {
        if (auto mutexValue = mutexWeakPtr.lock())
        {
            std::lock_guard<std::mutex> lockGuard{*mutexValue};
            std::lock_guard<std::mutex> lockGuardMap{m_mapMutex};
            auto result = m_deviceInstallationResult.find(deviceKey);
            if (result != m_deviceInstallationResult.cend())
                result->second = installation ? InstallResponse::INSTALLED : InstallResponse::FAILED_TO_INSTALL;
        }
        const auto cvIt = m_conditionVariables.find(fileName);
        if (cvIt != m_conditionVariables.cend())
            cvIt->second->notify_one();
    });

    // Wait for the installation to be done
    if (installing)
    {
        std::unique_lock<std::mutex> lock{*mutex};
        conditionVariable->wait_for(lock, std::chrono::seconds{120});
    }
    else
    {
        m_deviceInstallationResult[deviceKey] = InstallResponse::FAILED_TO_INSTALL;
    }
    // Remove the values from the maps
    {
        std::lock_guard<std::mutex> lock{m_mapMutex};
        m_devicesInstallingFiles.erase(deviceKey);
        m_conditionVariables.erase(fileName);
        return m_deviceInstallationResult[deviceKey];
    }
}

void DebianPackageInstaller::abortFirmwareInstall(const std::string& deviceKey)
{
    // Check if there is a condition variable waiting for the device
    std::lock_guard<std::mutex> lock{m_mapMutex};
    const auto deviceIt = m_devicesInstallingFiles.find(deviceKey);
    if (deviceIt != m_devicesInstallingFiles.cend())
    {
        const auto cvIt = m_conditionVariables.find(deviceIt->second);
        if (cvIt != m_conditionVariables.cend())
        {
            cvIt->second->notify_one();
        }
    }
}

bool DebianPackageInstaller::wasFirmwareInstallSuccessful(const std::string&, const std::string&)
{
    return true;
}

std::string DebianPackageInstaller::getFirmwareVersion(const std::string&)
{
    LOG(TRACE) << METHOD_INFO;

    // Execute the command
    auto cmd = "dpkg -l | grep " + m_serviceName + " | tr -s ' ' | cut -d ' ' -f3";
    auto exec = std::unique_ptr<FILE, decltype(&pclose)>{popen(cmd.c_str(), "r"), pclose};
    if (exec == nullptr)
        return {};

    // Copy over the result into a string
    auto result = std::string{};
    char buffer[128];
    while (fgets(buffer, 128, exec.get()) != nullptr)
        result += buffer;
    while (result.find('\n') != std::string::npos)
        result.replace(result.find('\n'), 1, "");
    return result;
}
}    // namespace wolkabout::connect

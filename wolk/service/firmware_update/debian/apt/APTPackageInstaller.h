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

#ifndef WOLKABOUTCONNECTOR_APTPACKAGEINSTALLER_H
#define WOLKABOUTCONNECTOR_APTPACKAGEINSTALLER_H

#include "wolk/service/firmware_update/debian/GenericDBusInterface.h"

#include <string>
#include <unordered_map>

namespace wolkabout
{
namespace connect
{
/**
 * This enumeration is used to represent the installation success status.
 */
enum class InstallationResult
{
    Installed,
    FailedToConnectToAPT,
    PackageNotFound,
};

/**
 * This is a utility method that is used to convert an InstallationResult value into a string.
 *
 * @param result Enumeration value.
 * @return String representation for the enumeration value.
 */
std::string toString(InstallationResult result);

// Here we have a type alias for a callback that is invoked once an installation is over.
using InstallationCallback = std::function<void(const std::string&, InstallationResult)>;

/**
 * This class implements a package installer for APT. This allows the software to install a package using a DBus
 * connection.
 */
class APTPackageInstaller
{
public:
    /**
     * This is the method that allows the user to install an debian package using APT.
     * This method is entirely asynchronous as the installation status will be reported to us.
     *
     * @param absolutePath The absolute path to the debian package that is supposed to be installed.
     * @param callback The callback that will be invoked once the installation is resolved.
     * @return Whether the installation request has been successfully made. If this is false, the callback will never be
     * called.
     */
    bool installPackage(const std::string& absolutePath,
                        std::function<void(const std::string&, InstallationResult)> callback);

private:
    /**
     * This is an internal method that is used to handle the Finished signal being called.
     *
     * @param objectPath The object which invoked the Finished signal.
     * @param value The value that was sent with the Finished signal.
     */
    void handleFinishedSignal(const std::string& objectPath, GVariant* value);

    /**
     * This is an internal method that is used to handle the ConfigFileConflict signal being called.
     *
     * @param objectPath The object which invoked the ConfigFileConflict signal.
     * @param value The value that was sent with the ConfigFileConflict signal.
     */
    void handleConfigFileConflict(const std::string& objectPath, GVariant* value);

    // The logger tag
    const std::string TAG = "[APTPackageInstaller] -> ";

    // Here we store the DBus connection
    std::mutex m_connectionMutex;
    GenericDBusInterface m_dbusConnection;

    // Here we store the map of transactions
    std::unordered_map<std::string, std::string> m_transactions;
    std::unordered_map<std::string, std::function<void(const std::string&, InstallationResult)>> m_callbacks;

    // And a command buffer where we execute callback
    CommandBuffer m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_APTPACKAGEINSTALLER_H

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

#ifndef WOLKABOUTCONNECTOR_FIRMWAREINSTALLER_H
#define WOLKABOUTCONNECTOR_FIRMWAREINSTALLER_H

#include <string>

namespace wolkabout
{
namespace connect
{
/**
 * This is an enumeration containing all responses the user can return when the `installFirmware` command was
 * invoked.
 */
enum class InstallResponse
{
    FAILED_TO_INSTALL,
    NO_FILE,
    WILL_INSTALL,
    INSTALLED,
};

/**
 * This is an interface for a class capable of installing firmware on command from the platform.
 */
class FirmwareInstaller
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~FirmwareInstaller() = default;

    /**
     * This is the method with which the service notifies the user implemented FirmwareInstaller that an
     * installation command has been received.
     *
     * @param fileName The name of the file
     */
    virtual InstallResponse installFirmware(const std::string& deviceKey, const std::string& fileName) = 0;

    /**
     * This is the method that is invoked when the platform wants to abort a currently ongoing firmware installation
     * session.
     */
    virtual void abortFirmwareInstall(const std::string& deviceKey) = 0;

    /**
     * This is the method with which the service will ask the firmware installer, if the firmware install was
     * successful. This will get called after a reboot, when `installFirmware` was invoked.
     *
     * @return Whether the firmware install was successful.
     */
    virtual bool wasFirmwareInstallSuccessful(const std::string& deviceKey, const std::string& oldVersion);

    /**
     * This is the method with which the service can ask for the current firmware version.
     *
     * @return The current firmware version.
     */
    virtual std::string getFirmwareVersion(const std::string& deviceKey) = 0;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FIRMWAREINSTALLER_H

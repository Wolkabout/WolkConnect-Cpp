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

#ifndef WOLKABOUTCONNECTOR_SYSTEMDSERVICEINTERFACE_H
#define WOLKABOUTCONNECTOR_SYSTEMDSERVICEINTERFACE_H

#include "core/utility/Service.h"
#include "wolk/service/firmware_update/debian/GenericDBusInterface.h"

#include <string>

namespace wolkabout
{
namespace connect
{
/**
 * This enumeration is to represent the service restart status.
 */
enum class ServiceRestartResult
{
    FailedToFindToDBus,
    FailedToFindService,
    Successful
};

/**
 * This is a utility method that is used to convert a ServiceRestartResult value into a string.
 *
 * @param result Enumeration value.
 * @return String representation for the enumeration value.
 */
std::string toString(ServiceRestartResult result);

/**
 * This class implements a systemd service manager. This allows the software to interact with systemd services using a
 * DBus connection.
 */
class SystemdServiceInterface : public Service
{
public:
    /**
     * Default overridden destructor. Calls the stop method.
     */
    ~SystemdServiceInterface() override;

    /**
     * Overridden method from the `wolkabout::Service` interface.
     * This will establish the DBus connection.
     */
    void start() override;

    /**
     * Overridden method from the `wolkabout::Service` interface.
     * This will close the DBus connection.
     */
    void stop() override;

    /**
     * This is the method that allows the user to restart a service.
     *
     * @param serviceObjectName The name of an service unit object.
     * @return Whether the installation request has been successfully made. If it is not `Successful`, it is not
     * successful.
     */
    virtual ServiceRestartResult restartService(const std::string& serviceObjectName);

    /**
     * This is the method that allows the user to obtain the object name of a service - if it exists.
     *
     * @param serviceName The name of the service the user is querying about.
     * @return The name of the object which represents the service. Empty if the service does not exist.
     */
    virtual std::string obtainObjectNameForService(std::string serviceName);

protected:
    // The logger tag
    const std::string TAG = "[SystemdServiceInterface] -> ";

    // Here we store the DBus connection
    std::mutex m_connectionMutex;
    GenericDBusInterface m_dbusConnection;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_SYSTEMDSERVICEINTERFACE_H

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

#include "wolk/service/firmware_update/debian/systemd/SystemdServiceInterface.h"

#include "core/utilities/Logger.h"

namespace wolkabout
{
namespace connect
{
const std::string SYSTEMD_NAMESPACE = "org.freedesktop.systemd1";
const std::string SYSTEMD_MANAGER_OBJECT = "/org/freedesktop/systemd1";
const std::string SYSTEMD_MANAGER_INTERFACE = "org.freedesktop.systemd1.Manager";
const std::string SYSTEMD_UNIT_INTERFACE = "org.freedesktop.systemd1.Unit";
const std::string SYSTEMD_LOAD_UNIT_METHOD = "LoadUnit";

SystemdServiceInterface::~SystemdServiceInterface()
{
    stop();
}

void SystemdServiceInterface::start()
{
    LOG(TRACE) << METHOD_INFO;

    // Attempt to connect
    std::lock_guard<std::mutex> lockGuard{m_connectionMutex};
    if (!m_dbusConnection.connect("SYSTEM_BUS"))
    {
        LOG(ERROR) << TAG << "Failed to connect to DBUS/SYSTEM_BUS.";
        return;
    }
}

void SystemdServiceInterface::stop()
{
    LOG(TRACE) << METHOD_INFO;

    // Disconnect
    std::lock_guard<std::mutex> lockGuard{m_connectionMutex};
    m_dbusConnection.disconnect();
}

ServiceRestartResult SystemdServiceInterface::restartService(const std::string& serviceObjectName)
{
    LOG(TRACE) << METHOD_INFO;

    try
    {
        // Invoke the method
        auto value = g_variant_new_string("replace");
        auto tuple = g_variant_new_tuple(&value, 1);
        const auto response =
          m_dbusConnection.callMethod(SYSTEMD_NAMESPACE, serviceObjectName, SYSTEMD_UNIT_INTERFACE, "Restart", tuple);
        LOG(INFO) << "Received response for 'Restart' method: "
                  << g_variant_get_string(g_variant_get_child_value(response, 0), nullptr) << "'.";
        return ServiceRestartResult::Successful;
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << TAG << "Failed to invoke the DBus method: '" << exception.what() << "'.";
        return ServiceRestartResult::FailedToFindToDBus;
    }
}

std::string SystemdServiceInterface::obtainObjectNameForService(std::string serviceName)
{
    LOG(TRACE) << METHOD_INFO;

    // Check the service name
    if (serviceName.find(".service") == std::string::npos)
        serviceName += ".service";

    try
    {
        // Invoke the method
        auto builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
        g_variant_builder_add_value(builder, g_variant_new_string(serviceName.c_str()));
        auto payload = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
        const auto response = m_dbusConnection.callMethod(SYSTEMD_NAMESPACE, SYSTEMD_MANAGER_OBJECT,
                                                          SYSTEMD_MANAGER_INTERFACE, SYSTEMD_LOAD_UNIT_METHOD, payload);
        return {g_variant_get_string(g_variant_get_child_value(response, 0), nullptr)};
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << TAG << "Failed to invoke the DBus method: '" << exception.what() << "'.";
        return {};
    }
}
}    // namespace connect
}    // namespace wolkabout

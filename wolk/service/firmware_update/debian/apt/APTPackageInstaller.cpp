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

#include "wolk/service/firmware_update/debian/apt/APTPackageInstaller.h"

#include "core/utilities/Logger.h"

namespace wolkabout
{
namespace connect
{
const std::string APT_NAMESPACE = "org.debian.apt";
const std::string APT_OBJECT = "/org/debian/apt";
const std::string APT_INTERFACE = "org.debian.apt";
const std::string APT_TRANSACTION_INTERFACE = "org.debian.apt.transaction";
const std::string APT_INSTALL_METHOD = "InstallPackages";
const std::string APT_RUN_METHOD = "Run";
const std::string APT_RESOLVE_CONFIG_CONFLICT_METHOD = "ResolveConfigFileConflict";
const std::string APT_FINISHED_SIGNAL = "Finished";
const std::string APT_CONFIG_CONFLICT_SIGNAL = "ConfigFileConflict";

std::string toString(InstallationResult result)
{
    switch (result)
    {
    case InstallationResult::Installed:
        return "Installed";
    case InstallationResult::FailedToConnectToAPT:
        return "FailedToConnectToAPT";
    case InstallationResult::PackageNotFound:
        return "PackageNotFound";
    default:
        return "";
    }
}

bool APTPackageInstaller::installPackage(const std::string& absolutePath,
                                         std::function<void(const std::string&, InstallationResult)> callback)
{
    LOG(TRACE) << METHOD_INFO;

    // Open the connection to the system bus on the machine
    std::lock_guard<std::mutex> lockGuard{m_connectionMutex};
    if (!m_dbusConnection.connect("SYSTEM_BUS"))
    {
        LOG(ERROR) << TAG << "Failed to connect to DBUS/SYSTEM_BUS.";
        return false;
    }

    // Now call the method that will create a transaction
    GVariant* payload;
    auto transactionObjectName = std::string{};
    {
        // Create the payload that will serve as the arguments
        const auto param = g_variant_new_string(absolutePath.c_str());
        const auto array = g_variant_new_array(G_VARIANT_TYPE_STRING, &param, 1);
        payload = g_variant_new_tuple(&array, 1);
    }
    try
    {
        // Call the method
        const auto transactionVariant =
          m_dbusConnection.callMethod(APT_NAMESPACE, APT_OBJECT, APT_INTERFACE, APT_INSTALL_METHOD, payload);
        if (transactionVariant == nullptr)
        {
            LOG(ERROR) << TAG << "Invoked apt installation method, but received no response.";
            return false;
        }

        // Extract the transaction object name
        char* transactionObjectNameCStr = nullptr;
        g_variant_get(transactionVariant, "(s)", &transactionObjectNameCStr);
        transactionObjectName = std::string(transactionObjectNameCStr);
        delete transactionObjectNameCStr;
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "Failed to invoke the method to create a transaction -> '" << exception.what() << "'.";
        return false;
    }

    try
    {
        // Now subscribe to the transaction object and await the response from it
        LOG(TRACE) << "Received transaction object '" << transactionObjectName << "'.";
        const auto signalSubscription = m_dbusConnection.subscribeToSignal(
          APT_NAMESPACE, transactionObjectName, APT_TRANSACTION_INTERFACE, APT_FINISHED_SIGNAL,
          [&](const std::string&, const std::string& objectPath, const std::string&, const std::string&,
              GVariant* value) { this->handleFinishedSignal(objectPath, value); });
        if (signalSubscription == 0)
        {
            LOG(ERROR) << "Failed to subscribe to the transaction's '" << APT_FINISHED_SIGNAL
                       << "' signal. Aborting...";
            return false;
        }
        if (m_dbusConnection.subscribeToSignal(
              APT_NAMESPACE, transactionObjectName, APT_TRANSACTION_INTERFACE, APT_CONFIG_CONFLICT_SIGNAL,
              [&](const std::string&, const std::string& objectPath, const std::string&, const std::string&,
                  GVariant* value) { this->handleConfigFileConflict(objectPath, value); }) == 0)
        {
            LOG(WARN) << "Failed to subscribe to the transaction's '" << APT_CONFIG_CONFLICT_SIGNAL << "' signal.";
        }
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "Failed to invoke the subscribe to the signals of the subscription -> '" << exception.what()
                   << "'.";
        return false;
    }

    try
    {
        m_dbusConnection.callMethod(APT_NAMESPACE, transactionObjectName, APT_TRANSACTION_INTERFACE, APT_RUN_METHOD,
                                    nullptr);
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "Failed to invoke the method to run the transaction -> '" << exception.what() << "'.";
        return false;
    }

    m_transactions.emplace(transactionObjectName, absolutePath);
    m_callbacks.emplace(transactionObjectName, std::move(callback));

    // Now interact with the transaction object
    return true;
}

void APTPackageInstaller::handleFinishedSignal(const std::string& objectPath, GVariant* value)
{
    LOG(TRACE) << METHOD_INFO;

    // Obtain the result string
    const auto result = std::string(g_variant_get_string(g_variant_get_child_value(value, 0), nullptr));
    const auto callbackIt = m_callbacks.find(objectPath);
    const auto pathIt = m_transactions.find(objectPath);
    if (callbackIt != m_callbacks.cend() && pathIt != m_transactions.cend())
    {
        const auto callback = m_callbacks[objectPath];
        const auto absolutePath = m_transactions[objectPath];
        m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>([absolutePath, callback, result] {
            callback(absolutePath,
                     result.empty() ? InstallationResult::Installed : InstallationResult::PackageNotFound);
        }));
        m_callbacks.erase(objectPath);
        m_transactions.erase(objectPath);
    }
}

void APTPackageInstaller::handleConfigFileConflict(const std::string& objectPath, GVariant* value)
{
    LOG(TRACE) << METHOD_INFO;

    // Extract the old and new name of the config
    char *oldConfigName, *newConfigName;
    g_variant_get(value, "(ss)", &oldConfigName, &newConfigName);

    // Invoke the method on the object to resolve the conflict
    const auto builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
    g_variant_builder_add_value(builder, g_variant_new_string(oldConfigName));
    g_variant_builder_add_value(builder, g_variant_new_string("keep"));
    auto tuple = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    m_dbusConnection.callMethod(APT_NAMESPACE, objectPath, APT_TRANSACTION_INTERFACE,
                                APT_RESOLVE_CONFIG_CONFLICT_METHOD, tuple);
}
}    // namespace connect
}    // namespace wolkabout

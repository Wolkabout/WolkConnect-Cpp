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

#include "wolk/service/firmware_update/debian/GenericDBusInterface.h"

#include "core/utilities/Logger.h"

#include <algorithm>
#include <utility>

namespace wolkabout
{
namespace connect
{
const std::string MAIN_NAMESPACE_NAME = "org.freedesktop.DBus";
const std::string MAIN_NAMESPACE_DBUS_OBJECT_NAME = "/org/freedesktop/DBus";
const std::string MAIN_INTERFACE_NAMESPACE_NAME = "org.freedesktop.DBus";
const std::string MAIN_NAMESPACE_GET_NAME_OWNER_METHOD_NAME = "GetNameOwner";
const std::string PROPERTIES_INTERFACE_NAME = "org.freedesktop.DBus.Properties";
const std::string PROPERTIES_CHANGED_SIGNAL_NAME = "PropertiesChanged";
const std::string PROPERTIES_GET_METHOD_NAME = "Get";
const std::string PROPERTIES_GET_ALL_METHOD_NAME = "GetAll";
const std::string PROPERTIES_SET_METHOD_NAME = "Set";

void callbackPointer(GDBusConnection*, const gchar* sender_name, const gchar* object_path, const gchar* interface_name,
                     const gchar* signal_name, GVariant* parameters, gpointer user_data)
{
    LOG(TRACE) << "Received signal callback -> (" << sender_name << ", " << object_path << ", " << interface_name
               << ", " << signal_name << ", Value size : " << std::to_string(g_variant_get_size(parameters)) << ")";

    try
    {
        /**
         * Look for the `GenericDBusInterface` object that is used this method as the subscription via the user_data
         * pointer. If it's valid, cast it to the object and pass it data that was received.
         */
        if (user_data != nullptr)
        {
            const auto instance = static_cast<GenericDBusInterface*>(user_data);
            instance->externalSignalCallback(sender_name, object_path, interface_name, signal_name, parameters);
        }
    }
    catch (std::exception& e)
    {
        LOG(ERROR) << "Error while invoking DBus interface signal callback - " << e.what();
    }
}

void nameWatcher(GDBusConnection*, const gchar* name, const gchar* name_owner, gpointer user_data)
{
    LOG(TRACE) << "Received name watcher -> (" << name << ", " << name_owner << ")";

    try
    {
        if (user_data != nullptr)
        {
            const auto instance = static_cast<GenericDBusInterface*>(user_data);
            instance->externalNameWatch(name, name_owner);
        }
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "Error while invoking DBus interface name watching - " << exception.what();
    }
}

GenericDBusInterface::GenericDBusInterface() : m_dbusConnection{nullptr}, m_mainLoop{nullptr} {}

GenericDBusInterface::~GenericDBusInterface()
{
    // If the connection is ongoing, close it.
    if (m_dbusConnection != nullptr)
    {
        // Unsubscribe from all the signals.
        auto signalSubscriptions = std::vector<std::uint32_t>{};
        for (const auto& kvp : m_signalCallbacks)
            signalSubscriptions.emplace_back(std::get<0>(kvp.second));
        for (const auto& subscription : signalSubscriptions)
            unsubscribeFromSignal(subscription);

        // Call the internal disconnect method.
        disconnect();
        m_dbusConnection = nullptr;
    }

    // Stop the main loop.
    stopLoop();
}

bool GenericDBusInterface::connect(const std::string& path)
{
    try
    {
        if (path == "SYSTEM_BUS")
        {
            LOG(INFO) << "Establishing a connection to the SYSTEM BUS.";

            // Prepare logic to connect to the system bus, and be ready to catch the error.
            GError* error = nullptr;
            m_dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
            if (m_dbusConnection == nullptr)
            {
                throw std::runtime_error(std::string(error->message));
            }
            LOG(INFO) << "Successfully established a connection to the SYSTEM BUS.";
        }
        else if (path == "SESSION_BUS")
        {
            LOG(INFO) << "Establishing a connection to the SESSION BUS.";

            // Prepare logic to connect to the system bus, and be ready to catch the error.
            GError* error = nullptr;
            m_dbusConnection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
            if (m_dbusConnection == nullptr)
            {
                throw std::runtime_error(std::string(error->message));
            }
            LOG(INFO) << "Successfully established a connection to the SESSION BUS.";
        }
        else
        {
            LOG(INFO) << "Establishing a connection to the path '" << path << "'.";

            // Check the address for the path
            if (!g_dbus_is_address(path.c_str()))
            {
                throw std::runtime_error("The given bus address is not a valid address.");
            }

            // Attempt to create the connection with the address that is passed
            GError* error = nullptr;
            m_dbusConnection = g_dbus_connection_new_for_address_sync(
              path.c_str(), G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, nullptr, nullptr, &error);
            if (m_dbusConnection == nullptr)
            {
                throw std::runtime_error(std::string(g_quark_to_string(error->domain)) + "(" +
                                         std::to_string(error->code) + ")");
            }
            LOG(INFO) << "Successfully established a connection to the path '" << path << "'.";
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "Error occurred while establishing a connection to a bus - " << exception.what();
        return false;
    }
}

bool GenericDBusInterface::disconnect()
{
    try
    {
        // Attempt to disconnect, check that the error is not null
        GError* connectionCloseError = nullptr;
        if (!g_dbus_connection_close_sync(m_dbusConnection, nullptr, &connectionCloseError) &&
            connectionCloseError != nullptr)
        {
            throw std::runtime_error(std::string(connectionCloseError->message));
        }

        // Set the dbus connection to null
        m_dbusConnection = nullptr;
        return true;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Error occurred while disconnecting from a bus - " << e.what();
        return false;
    }
}

void GenericDBusInterface::startLoop()
{
    m_mainLoop = g_main_loop_new(nullptr, false);
    if (m_mainLoop == nullptr)
        throw std::runtime_error("GenericDBusInterface can not be created - Failed to create a new GLib main loop!");

    g_main_loop_run(m_mainLoop);
}

void GenericDBusInterface::stopLoop()
{
    if (m_mainLoop != nullptr)
    {
        g_main_loop_quit(m_mainLoop);
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
    }
}

GVariant* GenericDBusInterface::callMethod(const std::string& dbusNamespace, const std::string& objectName,
                                           const std::string& interfaceName, const std::string& methodName,
                                           GVariant* parameters)
{
    // Check if the connection is open.
    if (!checkConnection())
    {
        const auto message = "DBus connection is not open!";
        LOG(ERROR) << "GenericDBusInterface method call failed - " << message;
        throw std::runtime_error(message);
    }

    // Call the method, and be ready to catch the error.
    GError* methodCallError = nullptr;
    const auto value = g_dbus_connection_call_sync(
      m_dbusConnection, dbusNamespace.c_str(), objectName.empty() ? nullptr : objectName.c_str(), interfaceName.c_str(),
      methodName.c_str(), parameters, nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &methodCallError);

    // Analyze the error if it occurred.
    if (methodCallError != nullptr)
    {
        const auto message = methodCallError->message;
        LOG(ERROR) << "GenericDBusInterface method call failed - " << message;
        throw std::runtime_error(message);
    }

    // Return if everything is okay.
    return value;
}

GVariant* GenericDBusInterface::getProperty(const std::string& dbusNamespace, const std::string& objectName,
                                            const std::string& interfaceName, const std::string& propertyName)
{
    try
    {
        // Make the parameter variant
        auto builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
        g_variant_builder_add_value(builder, g_variant_new_string(interfaceName.c_str()));
        g_variant_builder_add_value(builder, g_variant_new_string(propertyName.c_str()));
        auto parameters = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);

        // Call the method that requests the property value from the object
        // If the `callMethod` received null, it will throw an exception itself.
        const auto returnValue =
          callMethod(dbusNamespace, objectName, PROPERTIES_INTERFACE_NAME, PROPERTIES_GET_METHOD_NAME, parameters);
        return returnValue;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Error occurred while obtaining a property value of an object - " << e.what();
        return nullptr;
    }
}

bool GenericDBusInterface::setProperty(const std::string& dbusNamespace, const std::string& objectName,
                                       const std::string& interfaceName, const std::string& propertyName,
                                       GVariant* value)
{
    try
    {
        // Make the parameter variant. Include the passed parameter in the tuple.
        const auto builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
        g_variant_builder_add_value(builder, g_variant_new_string(interfaceName.c_str()));
        g_variant_builder_add_value(builder, g_variant_new_string(propertyName.c_str()));
        g_variant_builder_add_value(builder, value);
        const auto parameters = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);

        // Call the method that sets the property value for the object
        // This method will throw an exception itself if an error has occurred
        // The method does not return any value, the success is measured by no thrown exceptions
        callMethod(dbusNamespace, objectName, PROPERTIES_INTERFACE_NAME, PROPERTIES_SET_METHOD_NAME, parameters);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Error occurred while setting a property value - " << e.what();
        return false;
    }
}

GVariant* GenericDBusInterface::getAllProperties(const std::string& dbusNamespace, const std::string& objectName,
                                                 const std::string& interfaceName)
{
    try
    {
        // Make the parameter variant
        const auto builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
        g_variant_builder_add_value(builder, g_variant_new_string(interfaceName.c_str()));
        const auto parameters = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);

        // Call the method that will obtain all the parameter values of the object from one interface
        // This method will throw an exception itself if an error has occurred
        const auto returnValue =
          callMethod(dbusNamespace, objectName, PROPERTIES_INTERFACE_NAME, PROPERTIES_GET_ALL_METHOD_NAME, parameters);
        return returnValue;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Error occurred while obtaining all property values from an interface of an object - "
                   << e.what();
        return nullptr;
    }
}

uint32_t GenericDBusInterface::subscribeToSignal(const std::string& dbusNamespace, const std::string& objectName,
                                                 const std::string& interfaceName, const std::string& signalName,
                                                 const SignalCallback& callback)
{
    try
    {
        // Check if the connection is open.
        if (!checkConnection())
        {
            throw std::runtime_error("DBus connection is not open!");
        }

        // Subscribe to the signal.
        const auto subscriptionID =
          g_dbus_connection_signal_subscribe(m_dbusConnection, dbusNamespace.c_str(), interfaceName.c_str(),
                                             signalName.c_str(), objectName.empty() ? nullptr : objectName.c_str(),
                                             nullptr, G_DBUS_SIGNAL_FLAGS_NONE, callbackPointer, this, nullptr);

        // Mark it in the map.
        m_signalCallbacks.emplace(SignalIdentification(dbusNamespace, objectName, interfaceName, signalName),
                                  SignalSubscription(subscriptionID, callback));

        // Get the unique name for the namespace.
        subscribeToName(dbusNamespace);

        return subscriptionID;
    }
    catch (std::exception& e)
    {
        LOG(ERROR) << "Error occurred while subscribing to signal - " << std::string(e.what());
        return 0;
    }
}

bool GenericDBusInterface::unsubscribeFromSignal(uint32_t subscriptionId)
{
    try
    {
        // Check if the connection is open.
        if (!checkConnection())
        {
            throw std::runtime_error("DBus connection is not open!");
        }

        // We first need to check whether the subscription ID is in our recorded subscriptions
        const auto it = std::find_if(m_signalCallbacks.cbegin(), m_signalCallbacks.cend(),
                                     [&](const std::pair<SignalIdentification, SignalSubscription>& value) {
                                         return std::get<0>(value.second) == subscriptionId;
                                     });
        if (it == m_signalCallbacks.cend())
        {
            throw std::runtime_error("The subscription ID that was given is not in our subscriptions database!");
        }

        // Unsubscribe from the signal
        g_dbus_connection_signal_unsubscribe(m_dbusConnection, subscriptionId);
        m_signalCallbacks.erase(it);
        return true;
    }
    catch (const std::exception& exception)
    {
        LOG(ERROR) << "Error while unsubscribing from a signal - " << exception.what();
        return false;
    }
}

void GenericDBusInterface::externalSignalCallback(const std::string& dbusNamespace, const std::string& objectName,
                                                  const std::string& interfaceName, const std::string& signalName,
                                                  GVariant* value)
{
    // Find the well known name
    const auto nameIt =
      std::find_if(m_nameTracking.cbegin(), m_nameTracking.cend(),
                   [&](const std::pair<std::string, std::string>& pair) { return pair.second == dbusNamespace; });
    if (nameIt == m_nameTracking.cend())
    {
        LOG(ERROR) << "Received signal callback for a namespace we are not tracking!";
        return;
    }

    // Try to create the matching SignalIdentification, to find the callback that needs to be called with the new data
    const auto matchingTuple = SignalIdentification(nameIt->first, objectName, interfaceName, signalName);
    const auto it = m_signalCallbacks.find(matchingTuple);
    if (it == m_signalCallbacks.cend())
    {
        throw std::runtime_error("The subscription with the passed identifier was not found!");
    }

    // Copy over the GVariant
    auto copy = g_variant_byteswap(value);

    try
    {
        // Take the callback and call the function
        const auto callback = std::get<1>(it->second);
        m_commandBuffer.pushCommand(std::make_shared<std::function<void()>>([=]() {
            callback(nameIt->first, objectName, interfaceName, signalName, copy);
            g_variant_unref(copy);
        }));
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("The callback threw an error while processing the signal data - " +
                                 std::string(e.what()));
    }
}

void GenericDBusInterface::externalNameWatch(const std::string& name, const std::string& nameOwner)
{
    // Append into the map the name watch if it does not exist, and change if it does
    const auto it = m_nameTracking.find(name);
    if (it == m_nameTracking.cend())
        m_nameTracking.emplace(name, nameOwner);
    else
        m_nameTracking[name] = nameOwner;
}

bool GenericDBusInterface::checkConnection()
{
    return m_dbusConnection != nullptr && !g_dbus_connection_is_closed(m_dbusConnection);
}

std::string GenericDBusInterface::getNameOwner(const std::string& namespaceName)
{
    // Build the parameters object with the name of the namespace we need the owner's name
    const auto builder = g_variant_builder_new(G_VARIANT_TYPE_TUPLE);
    g_variant_builder_add_value(builder, g_variant_new_string(namespaceName.c_str()));
    const auto parameters = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);

    // Call the method
    const auto value = callMethod(MAIN_NAMESPACE_NAME, MAIN_NAMESPACE_DBUS_OBJECT_NAME, MAIN_INTERFACE_NAMESPACE_NAME,
                                  MAIN_NAMESPACE_GET_NAME_OWNER_METHOD_NAME, parameters);

    // Take the result
    if (value == nullptr)
    {
        throw std::runtime_error("Received return for the method call is null!");
    }
    return {g_variant_get_string((g_variant_get_child_value(value, 0)), nullptr)};
}

void GenericDBusInterface::subscribeToName(const std::string& namespaceName)
{
    // Add the watch
    g_bus_watch_name_on_connection(m_dbusConnection, namespaceName.c_str(), G_BUS_NAME_WATCHER_FLAGS_NONE, nameWatcher,
                                   nullptr, this, nullptr);

    // And acquire the name for now and set it into the map
    m_nameTracking.emplace(namespaceName, getNameOwner(namespaceName));
}

GenericDBusInterface::GenericDBusInterface(int) : m_dbusConnection{nullptr}, m_mainLoop{nullptr} {}
}    // namespace connect
}    // namespace wolkabout

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

#ifndef WOLKABOUTCONNECTOR_GENERICDBUSINTERFACE_H
#define WOLKABOUTCONNECTOR_GENERICDBUSINTERFACE_H

#include "core/utility/CommandBuffer.h"

#include <functional>
#include <gio/gio.h>
#include <glib.h>
#include <map>
#include <memory>
#include <string>

namespace wolkabout
{
namespace connect
{
/**
 * Typedef representing an object in which all the parameters needed to identify a signal can be stored.
 */
using SignalIdentification = std::tuple<std::string, std::string, std::string, std::string>;

/**
 * Typedef representing std::function type used to subscribe to signals. Using std::function allows creating [&]
 * lambdas, and doesn't require a (void*) user pointer to be able to do something.
 */
using SignalCallback =
  std::function<void(const std::string&, const std::string&, const std::string&, const std::string&, GVariant*)>;

/**
 * Typedef representing an object that contains the necessary information for a signal subscription, the ID that was
 * provided when the subscription was established by the bus, and the function that will be called as a callback when a
 * value for the signal has been received.
 */
using SignalSubscription = std::pair<uint32_t, SignalCallback>;

/**
 * This is the static function used to pass to GLib internal calls for subscriptions, which will then return the value
 * to the interface that is subscribed, which will handle the subscription.
 */
void callbackPointer(GDBusConnection* connection, const gchar* sender_name, const gchar* object_path,
                     const gchar* interface_name, const gchar* signal_name, GVariant* parameters, gpointer user_data);

/**
 * This is the static function used to pass to GLib internal calls for name watching, which will track which object owns
 * the well known name found in on the bus.
 */
void nameWatcher(GDBusConnection* connection, const gchar* name, const gchar* name_owner, gpointer user_data);

/**
 * Class representing a DBus connection, with generic methods for calling methods/subscribing to signals/accessing
 * properties of any namespace/object on the bus.
 */
class GenericDBusInterface
{
public:
    /**
     * Default constructor for the Interface. This will start the main loop for the GLib library.
     */
    GenericDBusInterface();

    /**
     * Default destructor for the Interface.
     * Cleans up the subscriptions, closes the connection and stops the main loop.
     */
    virtual ~GenericDBusInterface();

    /**
     * Open the DBus connection. By default, the connection will be established to the local SYSTEM_BUS, but the path
     * can be overridden with the parameter. The path must be a valid DBus path (must be valid according to the
     * g_dbus_is_address function).
     *
     * @return whether connection was successfully established or not.
     */
    bool connect(const std::string& path = "");

    /**
     * Close the active DBus connection.
     * @return whether connection was successfully closed.
     */
    bool disconnect();

    /**
     * This is the method that starts the GLib main loop. This method will block the thread that this is started on, as
     * the main loop will take over the thread and execute events on the main loop.
     */
    void startLoop();

    /**
     * This is the method that stops the GLib main loop. This method can be executed from a thread that is different
     * from the one where the main loop is running, and by stopping the main loop, all subscriptions will stopped as all
     * the other events that require the main loop.
     */
    void stopLoop();

    /**
     * Generic method for calling a method on an object found on the bus. The method will check that the connection is
     * still open. The method will return a null if the method call was unsuccessful, and the reason is written out via
     * the logger, and an exception is thrown.
     *
     * @param dbusNamespace DBus namespace in which the object can be found.
     * @param objectName Name of the object, can be empty if root object is being targeted.
     * @param interfaceName Name of the interface, where the method can be found.
     * @param methodName Name of the method we want called.
     * @param parameters Variant containing all parameters wanted by the method.
     * @return The GVariant containing the response of the method.
     */
    GVariant* callMethod(const std::string& dbusNamespace, const std::string& objectName,
                         const std::string& interfaceName, const std::string& methodName, GVariant* parameters);

    /**
     * Generic method for obtaining the value of a property of an object found on the bus. The method will check that
     * the connection is still open. This method will return a null if the method call was unsuccessful, and the reason
     * is written out via the logger.
     *
     * @param dbusNamespace DBus namespace in which the object can be found.
     * @param objectName Name of the object, can be empty if root object is being targeted.
     * @param interfaceName Name of the interface with the property.
     * @param propertyName Name of the property we're looking for.
     * @return The GVariant containing the property value.
     */
    GVariant* getProperty(const std::string& dbusNamespace, const std::string& objectName,
                          const std::string& interfaceName, const std::string& propertyName);

    /**
     * Generic method for setting the value of a property of an object found on the bus. The method will check that the
     * connection is still open. This method will return a bool indicating if the operation was successful. The user
     * must be the one aware of the type of the value that needs to be set for the property.
     *
     * @param dbusNamespace DBus namespace in which the object can be found.
     * @param objectName Name of the object, can be empty if root object is being targeted.
     * @param interfaceName Name of the interface with the property.
     * @param propertyName Name of the property we're setting the value for.
     * @param value New value we want to set for the property.
     * @return A boolean value indicating whether the Set operation was successful or not.
     */
    bool setProperty(const std::string& dbusNamespace, const std::string& objectName, const std::string& interfaceName,
                     const std::string& propertyName, GVariant* value);

    /**
     * Generic method for obtaining all the property values of an object that it contains in one interface. The method
     * will check that the connection is still open. The method will return a Map<String, Variant> that contains
     * key-value pair presenting all the properties of the interface.
     *
     * @param dbusNamespace DBus namespace in which the object can be found.
     * @param objectName Name of the object, can be empty if root object is being targeted.
     * @param interfaceName Name of the interface, from which we catch all properties.
     * @return Tuple containing boolean value indicating whether the GetAll operation was successful or not,
     * and value of the properties in variant (if the boolean is set to true).
     */
    GVariant* getAllProperties(const std::string& dbusNamespace, const std::string& objectName,
                               const std::string& interfaceName);

    /**
     * Generic method for subscribing to a signal of an object that contains signals. The method will check that the
     * connection is still open. It requires a callback function that will be called every time the signal has been
     * called by the object, and a value will be passed along to the callback. The method returns the subscription ID
     * that was provided by the bus for this singular instance of a subscription.
     *
     * @param dbusNamespace DBus namespace in which the object can be found.
     * @param objectName Name of the object, can be empty if root object is being targeted.
     * @param interfaceName Name of the interface, where the signal can be found.
     * @param signalName Name of the signal we want to subscribe to.
     * @param callback Callback function called when the signal is raised by the DBus object.
     * @return The subscription ID of this specific subscription. Will return 0 if it hasn't subscribed.
     */
    uint32_t subscribeToSignal(const std::string& dbusNamespace, const std::string& objectName,
                               const std::string& interfaceName, const std::string& signalName,
                               const SignalCallback& callback);

    /**
     * This method is used to unsubscribe the current connection for a signal, using the subscribe ID that was provided
     * when subscribing to a signal.
     *
     * @param subscriptionId Subscription ID received when the subscription was established.
     * @return Boolean value indicating whether or not unsubscribing was successful.
     */
    bool unsubscribeFromSignal(uint32_t subscriptionId);

    /**
     * This is an external method used by the `callbackPointer` function to route the signal callback to the object
     * instance. This method is public, but should not be used by user.
     *
     * @param dbusNamespace DBus namespace in which the object can be found.
     * @param objectName Name of the object, which raised the signal.
     * @param interfaceName Name of the interface in which the signal is defined.
     * @param signalName Name of the signal being raised.
     * @param value Value with which the signal is being raised.
     */
    virtual void externalSignalCallback(const std::string& dbusNamespace, const std::string& objectName,
                                        const std::string& interfaceName, const std::string& signalName,
                                        GVariant* value);

    /**
     * This is an external method used by the `nameWatcher` function to route the new name owner to the object instance.
     * This method is public, but should not be used by user.
     *
     * @param name The well-known name that is being tracked by the user.
     * @param nameOwner The new object owner of the name.
     */
    virtual void externalNameWatch(const std::string& name, const std::string& nameOwner);

protected:
    /**
     * Internal method that is used to check if the connection to a bus is active.
     *
     * @return Whether the connection is active or not.
     */
    bool checkConnection();

    /**
     * Internal method used to obtain the unique name (object name) of a well-known name.
     * Unique name is by which the object will raise signals, and the user should be able to know it,
     * to identify signal calls, as the subscriptions are targeting the well-known name, but received with the owner
     * name of the well-known name. The method will throw an exception if it is unable to obtain the name.
     *
     * @param namespaceName The well-known name of namespace on bus.
     * @return String containing the unique name (object name) of the owner.
     */
    std::string getNameOwner(const std::string& namespaceName);

    /**
     * Internal method used to watch the unique name of a well-known name on the bus. Used to make subscriptions listen
     * to well-known names, and not to a unique objects.
     *
     * @param namespaceName The well-known name of namespace on bus.
     */
    void subscribeToName(const std::string& namespaceName);

    /**
     * DBus necessary things.
     */
    GDBusConnection* m_dbusConnection;
    GMainLoop* m_mainLoop;
    wolkabout::CommandBuffer m_commandBuffer;

    /**
     * Signal subscriptions.
     */
    std::map<SignalIdentification, SignalSubscription> m_signalCallbacks;

    /**
     * This is where we track the owners of well known names on the bus.
     */
    std::map<std::string, std::string> m_nameTracking;

    /*
     * For testing purposes only
     */
    explicit GenericDBusInterface(int);
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_GENERICDBUSINTERFACE_H

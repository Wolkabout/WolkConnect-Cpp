/**
 * Copyright 2021 Wolkabout s.r.o.
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

#ifndef WOLKABOUTCONNECTOR_WOLKINTERFACE_H
#define WOLKABOUTCONNECTOR_WOLKINTERFACE_H

#include <functional>

namespace wolkabout
{
// This is an alias for a lambda expression that can listen to the Wolk object's connection status.
using ConnectionStatusListener = std::function<void(bool)>;

/**
 * This is an enumeration for every type of Wolk interface that can be built.
 */
enum WolkInterfaceType
{
    SingleDevice,
    MultiDevice
};

/**
 * This is an interface class that represents one of the WolkInterface implementations.
 */
class WolkInterface
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~WolkInterface() = default;

    /**
     * This method will invoke the Wolk object to connect to the platform.
     */
    virtual void connect() = 0;

    /**
     * This method will invoke the Wolk object to disconnect from the platform.
     */
    virtual void disconnect() = 0;

    /**
     * This method is a getter for the connection status of this Wolk object with the platform.
     *
     * @return Connection status - {@code: true} if connected, {@code: false} if not.
     */
    virtual bool isConnected() = 0;

    /**
     * This method is a setter for a listener of the Wolk object's connection status.
     *
     * @param listener The new connection status listener.
     */
    virtual void setConnectionStatusListener(ConnectionStatusListener listener) = 0;

    /**
     * This method will invoke the Wolk object to publish everything that is held in persistence.
     * This includes readings, parameters and attributes for any devices that are presented via this Wolk object.
     */
    virtual void publish() = 0;

    /**
     * This method will return a value indicating which type of a Wolk instance is this object.
     *
     * @return The enumeration value indicating Wolk type.
     */
    virtual WolkInterfaceType getType() = 0;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_WOLKINTERFACE_H

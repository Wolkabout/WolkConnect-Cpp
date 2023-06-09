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

#ifndef WOLK_INTERFACE_H
#define WOLK_INTERFACE_H

#include "core/model/Reading.h"
#include "core/utilities/CommandBuffer.h"
#include "wolk/WolkInterfaceType.h"
#include "wolk/api/FeedUpdateHandler.h"
#include "wolk/api/ParameterHandler.h"
#include "wolk/service/data/DataService.h"
#include "wolk/service/error/ErrorService.h"
#include "wolk/service/file_management/FileManagementService.h"
#include "wolk/service/firmware_update/FirmwareUpdateService.h"
#include "wolk/service/platform_status/PlatformStatusService.h"
#include "wolk/service/registration_service/RegistrationService.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>

namespace wolkabout
{
// Forward declaring handlers, and some other helping objects
class InboundMessageHandler;
class OutboundMessageHandler;
class OutboundRetryMessageHandler;
class Persistence;

// Forward declaring protocols
class DataProtocol;
class FileManagementProtocol;
class FirmwareUpdateProtocol;
class PlatformStatusProtocol;
class RegistrationProtocol;

namespace connect
{
// Forward declare some API entities
class FileDownloader;
class FileListener;

// This is an alias for a lambda expression that can listen to the Wolk object's connection status.
using ConnectionStatusListener = std::function<void(bool)>;

/**
 * This is an interface class that represents a Wolk implementation.
 * A Wolk implementation is a further specialized
 */
class WolkInterface
{
    friend class WolkBuilder;

public:
    /**
     * Default virtual destructor.
     */
    virtual ~WolkInterface();

    /**
     * This method will invoke the Wolk object to connect to the platform.
     */
    virtual void connect();

    /**
     * This method will invoke the Wolk object to disconnect from the platform.
     */
    virtual void disconnect();

    /**
     * This method is a getter for the connection status of this Wolk object with the platform.
     *
     * @return Connection status - {@code: true} if connected, {@code: false} if not.
     */
    virtual bool isConnected();

    /**
     * This method is a setter for a listener of the Wolk object's connection status.
     *
     * @param listener The new connection status listener.
     */
    virtual void setConnectionStatusListener(ConnectionStatusListener listener);

    /**
     * This method will invoke the Wolk object to publish everything that is held in persistence.
     * This includes readings, parameters and attributes for any devices that are presented via this Wolk object.
     */
    virtual void publish();

    /**
     * This method will return a value indicating which type of a Wolk instance is this object.
     *
     * @return The enumeration value indicating Wolk type.
     */
    virtual WolkInterfaceType getType() const = 0;

protected:
    // Internal forward declaration for the class that will listen to the ConnectivityService.
    class ConnectivityFacade;

    // The protected constructor that will set the connection status to false, and create the command buffer.
    WolkInterface();

    // Here are some internal methods regarding the connection
    virtual void tryConnect(bool firstTime);
    virtual void notifyConnected();
    virtual void notifyDisconnected();
    virtual void notifyConnectionStatusListener();

    // Here are some internal methods used to publish data from persistence
    virtual void flushReadings();
    virtual void flushAttributes();
    virtual void flushParameters();

    // Here are internal methods that are used to propagate the data to external handlers
    virtual void handleFeedUpdateCommand(const std::string& deviceKey,
                                         const std::map<std::uint64_t, std::vector<Reading>>& readings);
    virtual void handleParameterCommand(const std::string& deviceKey, const std::vector<Parameter>& parameters);

    // Here are some utility methods to be used
    static std::uint64_t currentRtc();
    void addToCommandBuffer(std::function<void()> command);

    // Here is the place for the connection status and its listener
    std::atomic_bool m_connected;
    ConnectionStatusListener m_connectionStatusListener;

    // Here is the place for external entities capable of receiving Reading values.
    std::function<void(const std::string&, const std::map<std::uint64_t, std::vector<Reading>>)>
      m_feedUpdateHandlerLambda;
    std::weak_ptr<FeedUpdateHandler> m_feedUpdateHandler;

    // Here is the place for external entities capable of receiving Parameter values.
    std::function<void(const std::string&, const std::vector<Parameter>)> m_parameterLambda;
    std::weak_ptr<ParameterHandler> m_parameterHandler;

    // Here are entities related to an MQTT connection.
    std::unique_ptr<ConnectivityService> m_connectivityService;
    std::shared_ptr<InboundMessageHandler> m_inboundMessageHandler;
    OutboundMessageHandler* m_outboundMessageHandler;
    std::shared_ptr<OutboundRetryMessageHandler> m_outboundRetryMessageHandler;
    std::unique_ptr<Persistence> m_persistence;

    // List of all protocols the Wolk object must hold
    std::unique_ptr<DataProtocol> m_dataProtocol;
    std::unique_ptr<ErrorProtocol> m_errorProtocol;
    std::unique_ptr<FileManagementProtocol> m_fileManagementProtocol;
    std::unique_ptr<FirmwareUpdateProtocol> m_firmwareUpdateProtocol;
    std::unique_ptr<PlatformStatusProtocol> m_platformStatusProtocol;
    std::unique_ptr<RegistrationProtocol> m_registrationProtocol;

    // List of all services the Wolk object must hold
    std::shared_ptr<DataService> m_dataService;
    std::shared_ptr<ErrorService> m_errorService;
    std::shared_ptr<FileManagementService> m_fileManagementService;
    std::shared_ptr<FirmwareUpdateService> m_firmwareUpdateService;
    std::shared_ptr<PlatformStatusService> m_platformStatusService;
    std::shared_ptr<RegistrationService> m_registrationService;

    // Here is the command buffer that should be used
    std::unique_ptr<legacy::CommandBuffer> m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLK_INTERFACE_H

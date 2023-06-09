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

#ifndef WOLKABOUTCONNECTOR_PLATFORMSTATUSSERVICE_H
#define WOLKABOUTCONNECTOR_PLATFORMSTATUSSERVICE_H

#include "core/MessageListener.h"
#include "core/utilities/CommandBuffer.h"
#include "wolk/api/PlatformStatusListener.h"

#include <functional>

namespace wolkabout
{
class PlatformStatusProtocol;

namespace connect
{
/**
 * This is a service that is meant to receive information about platform connection status from the gateway.
 * This data is meant to be propagated further.
 */
class PlatformStatusService : public MessageListener
{
public:
    /**
     * Default constructor for the service that receives a listener object.
     *
     * @param protocol The protocol by which the service will oblige.
     * @param listener The listener object which will receive information.
     */
    PlatformStatusService(PlatformStatusProtocol& protocol, std::shared_ptr<PlatformStatusListener> listener);

    /**
     * This is an overridden method from the `MessageListener` interface.
     * This method is invoked everytime a message for this service arrives.
     *
     * @param message The message that arrived for the service.
     */
    void messageReceived(std::shared_ptr<Message> message) override;

    /**
     * This is an overridden method from the `MessageListener` interface.
     * This method is used to return the protocol that this service uses.
     *
     * @return The protocol reference.
     */
    const Protocol& getProtocol() override;

private:
    // Here we store the protocol given to us when the service was created.
    PlatformStatusProtocol& m_protocol;

    // Here we store the means of transporting the data further.
    std::shared_ptr<PlatformStatusListener> m_listener;

    // Here we have the command buffer that will execute external calls.
    legacy::CommandBuffer m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_PLATFORMSTATUSSERVICE_H

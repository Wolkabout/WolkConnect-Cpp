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

#ifndef WOLKABOUTCONNECTOR_ERRORSERVICE_H
#define WOLKABOUTCONNECTOR_ERRORSERVICE_H

#include "core/InboundMessageHandler.h"
#include "core/protocol/ErrorProtocol.h"

namespace wolkabout
{
class ErrorService : public MessageListener
{
public:
    explicit ErrorService(const ErrorProtocol& protocol);

    void messageReceived(std::shared_ptr<Message> message) override;

    const Protocol& getProtocol() override;

private:
    const ErrorProtocol& m_protocol;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_ERRORSERVICE_H

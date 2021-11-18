/*
 * Copyright 2021 Adriateh d.o.o.
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

#ifndef WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H
#define WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H

#include "core/InboundMessageHandler.h"

namespace wolkabout
{
class FileManagementService : public MessageListener
{
public:
    FileManagementService(Protocol& protocol, std::string fileLocation, std::uint64_t maxPacketSize);

    void messageReceived(std::shared_ptr<Message> message) override;
    const Protocol& getProtocol() override;

private:
    // This is where the protocol will be passed while the service is created.
    Protocol& m_protocol;

    // This is where the user parameters will be passed.
    std::string m_fileLocation;
    std::uint64_t m_maxPacketSize;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILEMANAGEMENTSERVICE_H

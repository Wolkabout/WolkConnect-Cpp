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

#include "core/protocol/Protocol.h"
#include "wolk/service/file_management/FileManagementService.h"

#include <utility>

namespace wolkabout
{
FileManagementService::FileManagementService(Protocol& protocol, std::string fileLocation, std::uint64_t maxPacketSize)
: m_protocol(protocol), m_fileLocation(std::move(fileLocation)), m_maxPacketSize(maxPacketSize)
{
}

const Protocol& FileManagementService::getProtocol()
{
    return m_protocol;
}

void FileManagementService::messageReceived(std::shared_ptr<Message> message) {}
}    // namespace wolkabout

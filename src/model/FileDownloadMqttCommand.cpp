/*
 * Copyright 2017 WolkAbout Technology s.r.o.
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

#include "FileDownloadMqttCommand.h"

namespace wolkabout
{
FileDownloadMqttCommand::FileDownloadMqttCommand() : m_type{FileDownloadMqttCommand::Type::INIT}
{
}

FileDownloadMqttCommand::FileDownloadMqttCommand(FileDownloadMqttCommand::Type type) : m_type{type}
{
}

FileDownloadMqttCommand::FileDownloadMqttCommand(FileDownloadMqttCommand::Type type,
												 WolkOptional<std::string> name,
												 WolkOptional<int> size,
												 WolkOptional<std::string> hash) :
	m_type{type}, m_name{name}, m_size{size}, m_hash{hash}
{
}

FileDownloadMqttCommand::Type FileDownloadMqttCommand::getType() const
{
	return m_type;
}

WolkOptional<std::string> FileDownloadMqttCommand::getName() const
{
    return m_name;
}

WolkOptional<int> FileDownloadMqttCommand::getSize() const
{
    return m_size;
}

WolkOptional<std::string> FileDownloadMqttCommand::getHash() const
{
    return m_hash;
}

}

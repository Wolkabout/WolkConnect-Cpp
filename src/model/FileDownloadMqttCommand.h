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

#ifndef FILEDOWNLOADMQTTCOMMAND_H
#define FILEDOWNLOADMQTTCOMMAND_H

#include "WolkOptional.h"
#include <string>

namespace wolkabout
{

class FileDownloadMqttCommand
{
public:
	enum class Type
	{
		INIT,
		END
	};

	FileDownloadMqttCommand();
	FileDownloadMqttCommand(FileDownloadMqttCommand::Type type);
    FileDownloadMqttCommand(FileDownloadMqttCommand::Type type, std::string name, int size, int count, std::string hash);

	FileDownloadMqttCommand::Type getType() const;

    WolkOptional<std::string> getName() const;
    WolkOptional<int> getSize() const;
    WolkOptional<int> getCount() const;
    WolkOptional<std::string> getHash() const;

private:
	FileDownloadMqttCommand::Type m_type;

    WolkOptional<std::string> m_name;
    WolkOptional<int> m_size;
    WolkOptional<int> m_count;
    WolkOptional<std::string> m_hash;
};

}

#endif // FILEDOWNLOADMQTTCOMMAND_H

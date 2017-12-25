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

#ifndef FILEDOWNLOADURLCOMMAND_H
#define FILEDOWNLOADURLCOMMAND_H

#include "WolkOptional.h"
#include <string>

namespace wolkabout
{
class FileDownloadUrlCommand
{
public:
	enum class Type
	{
		INIT = 0,
		STATUS,
		END,
		UPLOAD
	};

	FileDownloadUrlCommand();
	FileDownloadUrlCommand(FileDownloadUrlCommand::Type type);
	FileDownloadUrlCommand(FileDownloadUrlCommand::Type type, WolkOptional<std::string> url);

	FileDownloadUrlCommand::Type getType() const;

	WolkOptional<std::string> getUrl() const;

private:
	FileDownloadUrlCommand::Type m_type;

	WolkOptional<std::string> m_url;
};
}

#endif // FILEDOWNLOADURLCOMMAND_H

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

#include "FileDownloadUrlCommand.h"

namespace wolkabout
{

FileDownloadUrlCommand::FileDownloadUrlCommand() : m_type{FileDownloadUrlCommand::Type::INIT}, m_url{}
{
}

FileDownloadUrlCommand::FileDownloadUrlCommand(FileDownloadUrlCommand::Type type) : m_type{type}, m_url{}
{
}

FileDownloadUrlCommand::FileDownloadUrlCommand(FileDownloadUrlCommand::Type type, WolkOptional<std::string> url) :
	m_type{type}, m_url{url}
{
}

FileDownloadUrlCommand::Type FileDownloadUrlCommand::getType() const
{
	return m_type;
}

WolkOptional<std::string> FileDownloadUrlCommand::getUrl() const
{
	return m_url;
}

}

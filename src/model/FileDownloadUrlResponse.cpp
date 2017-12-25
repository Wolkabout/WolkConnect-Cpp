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

#include "FileDownloadUrlResponse.h"

namespace wolkabout
{
FileDownloadUrlResponse::FileDownloadUrlResponse() :
	m_status{FileDownloadUrlResponse::Status::OK}, m_command{FileDownloadUrlCommand::Type::STATUS},
	m_statusCode{}
{
}

FileDownloadUrlResponse::FileDownloadUrlResponse(
		FileDownloadUrlResponse::Status status, FileDownloadUrlCommand::Type command) :
	m_status{status}, m_command{command}, m_statusCode{}
{
}

FileDownloadUrlResponse::FileDownloadUrlResponse(
		FileDownloadUrlResponse::Status status, FileDownloadUrlCommand::Type command,
						FileDownloadUrlResponse::StatusCode statusCode) :
	m_status{status}, m_command{command}, m_statusCode{statusCode}
{
}

FileDownloadUrlResponse::Status FileDownloadUrlResponse::getStatus() const
{
	return m_status;
}

FileDownloadUrlCommand::Type FileDownloadUrlResponse::getCommand() const
{
	return m_command;
}

WolkOptional<FileDownloadUrlResponse::StatusCode> FileDownloadUrlResponse::getStatusCode() const
{
	return m_statusCode;
}
}

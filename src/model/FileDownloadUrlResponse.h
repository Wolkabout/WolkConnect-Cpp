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

#ifndef FILEDOWNLOADURLRESPONSE_H
#define FILEDOWNLOADURLRESPONSE_H

#include "WolkOptional.h"
#include "FileDownloadUrlCommand.h"

namespace wolkabout
{
class FileDownloadUrlResponse
{
public:
	enum class Status
	{
		OK = 0,
		IN_PROGRESS,
		COMPLETED,
		ERROR
	};

	enum class StatusCode
	{
		UNSPECIFIED_ERROR = 0,
		FILE_DOWNLOAD_DISABLED,
		FILE_HANDLING_ERROR,
		DOWNLOAD_NOT_INITIATED,
		DOWNLOAD_NOT_COMPLETED,
		DOWNLOAD_ALREADY_STARTED
	};

	FileDownloadUrlResponse();
	FileDownloadUrlResponse(FileDownloadUrlResponse::Status status, FileDownloadUrlCommand::Type command);
	FileDownloadUrlResponse(FileDownloadUrlResponse::Status status, FileDownloadUrlCommand::Type command,
							FileDownloadUrlResponse::StatusCode statusCode);

	FileDownloadUrlResponse::Status getStatus() const;
	FileDownloadUrlCommand::Type getCommand() const;
	WolkOptional<FileDownloadUrlResponse::StatusCode> getStatusCode() const;

private:
	FileDownloadUrlResponse::Status m_status;
	FileDownloadUrlCommand::Type m_command;
	WolkOptional<FileDownloadUrlResponse::StatusCode> m_statusCode;
};

}

#endif // FILEDOWNLOADURLRESPONSE_H

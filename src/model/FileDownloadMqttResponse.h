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

#ifndef FILEDOWNLOADMQTTRESPONSE_H
#define FILEDOWNLOADMQTTRESPONSE_H

#include "WolkOptional.h"

namespace wolkabout
{
class FileDownloadMqttResponse
{
public:
	enum class Status
	{
		OK = 0,
		ERROR
	};

	enum class ErrorCode
	{
		UNSPECIFIED_ERROR = 0,
		FILE_UPLOAD_DISABLED,
		PACKAGE_LENGTH_NOT_SUPPORTED,
		PACKAGE_HASH_INVALID,
		PREVIOUS_PACKAGE_HASH_INVALID,
		NOT_ALL_PACKAGES_RECEIVED,
		FILE_HANDLING_ERROR,
		FILE_TRANSFER_IN_PROGRESS,
		FILE_TRANSFER_NOT_INITIATED,
		FILE_CHECKSUM_INVALID
	};

	FileDownloadMqttResponse();
	FileDownloadMqttResponse(FileDownloadMqttResponse::Status status);
	FileDownloadMqttResponse(FileDownloadMqttResponse::Status status, FileDownloadMqttResponse::ErrorCode errorCode);
	FileDownloadMqttResponse(FileDownloadMqttResponse::Status status, int packageNumber);
	FileDownloadMqttResponse(FileDownloadMqttResponse::Status status, FileDownloadMqttResponse::ErrorCode errorCode,
							 int packageNumber);

	FileDownloadMqttResponse::Status getStatus() const;

	WolkOptional<FileDownloadMqttResponse::ErrorCode> getErrorCode() const;
	WolkOptional<int> getPackageNumber() const;

private:
	FileDownloadMqttResponse::Status m_status;
	WolkOptional<FileDownloadMqttResponse::ErrorCode> m_errorCode;
	WolkOptional<int> m_packageNumber;
};
}

#endif // FILEDOWNLOADMQTTRESPONSE_H

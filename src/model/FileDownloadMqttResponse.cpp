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

#include "FileDownloadMqttResponse.h"

namespace wolkabout
{
FileDownloadMqttResponse::FileDownloadMqttResponse() : m_status{FileDownloadMqttResponse::Status::OK},
	m_errorCode{}, m_packageNumber{}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(FileDownloadMqttResponse::Status status) : m_status{status},
	m_errorCode{}, m_packageNumber{}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(FileDownloadMqttResponse::Status status,
												   FileDownloadMqttResponse::ErrorCode errorCode) :
	m_status{status}, m_errorCode{errorCode}, m_packageNumber{}
{
}
FileDownloadMqttResponse::FileDownloadMqttResponse(FileDownloadMqttResponse::Status status, int packageNumber) :
	m_status{status}, m_errorCode{}, m_packageNumber{packageNumber}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(FileDownloadMqttResponse::Status status,
												   FileDownloadMqttResponse::ErrorCode errorCode,
												   int packageNumber) :
	m_status{status}, m_errorCode{errorCode}, m_packageNumber{packageNumber}
{
}

FileDownloadMqttResponse::Status FileDownloadMqttResponse::getStatus() const
{
	return m_status;
}

WolkOptional<FileDownloadMqttResponse::ErrorCode> FileDownloadMqttResponse::getErrorCode() const
{
	return m_errorCode;
}

WolkOptional<int> FileDownloadMqttResponse::getPackageNumber() const
{
	return m_packageNumber;
}
}

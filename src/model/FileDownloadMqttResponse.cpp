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
FileDownloadMqttResponse::FileDownloadMqttResponse() :
	FileDownloadMqttResponse(FileDownloadMqttResponse::Status::OK, FileDownloadMqttCommand::Type::STATUS)
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(
		FileDownloadMqttResponse::Status status, FileDownloadMqttCommand::Type command) :
	m_status{status}, m_command{command}, m_errorCode{}, m_packageNumber{},	m_packageSize{},
	m_packageCount{}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(
		FileDownloadMqttResponse::Status status, FileDownloadMqttCommand::Type command,
		WolkOptional<FileDownloadMqttResponse::ErrorCode> errorCode) :
	m_status{status}, m_command{command}, m_errorCode{errorCode}, m_packageNumber{},
	m_packageSize{}, m_packageCount{}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(
		FileDownloadMqttResponse::Status status, FileDownloadMqttCommand::Type command,
		WolkOptional<int> packageNumber) :
	m_status{status}, m_command{command}, m_errorCode{}, m_packageNumber{packageNumber},
	m_packageSize{}, m_packageCount{}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(
		FileDownloadMqttResponse::Status status, FileDownloadMqttCommand::Type command,
		WolkOptional<FileDownloadMqttResponse::ErrorCode> errorCode, WolkOptional<int> packageNumber) :
	m_status{status}, m_command{command}, m_errorCode{errorCode}, m_packageNumber{packageNumber},
	m_packageSize{}, m_packageCount{}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(
		FileDownloadMqttResponse::Status status, FileDownloadMqttCommand::Type command,
		WolkOptional<int> packageSize, WolkOptional<int> packageCount) :
	m_status{status}, m_command{command}, m_errorCode{}, m_packageNumber{}, m_packageSize{packageSize},
	m_packageCount{packageCount}
{
}

FileDownloadMqttResponse::FileDownloadMqttResponse(
		FileDownloadMqttResponse::Status status, FileDownloadMqttCommand::Type command,
		WolkOptional<FileDownloadMqttResponse::ErrorCode> errorCode, WolkOptional<int> packageNumber,
		WolkOptional<int> packageSize, WolkOptional<int> packageCount) :
	m_status{status}, m_command{command}, m_errorCode{errorCode}, m_packageNumber{packageNumber},
	m_packageSize{packageSize}, m_packageCount{packageCount}
{
}

FileDownloadMqttResponse::Status FileDownloadMqttResponse::getStatus() const
{
	return m_status;
}

FileDownloadMqttCommand::Type FileDownloadMqttResponse::getCommand() const
{
	return m_command;
}

WolkOptional<FileDownloadMqttResponse::ErrorCode> FileDownloadMqttResponse::getErrorCode() const
{
	return m_errorCode;
}

WolkOptional<int> FileDownloadMqttResponse::getPackageNumber() const
{
	return m_packageNumber;
}

WolkOptional<int> FileDownloadMqttResponse::getPackageSize() const
{
	return m_packageSize;
}

WolkOptional<int> FileDownloadMqttResponse::getPackageCount() const
{
	return m_packageCount;
}
}

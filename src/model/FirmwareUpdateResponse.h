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

#ifndef FIRMWAREUPDATERESPONSE_H
#define FIRMWAREUPDATERESPONSE_H

#include "WolkOptional.h"
#include "FirmwareUpdateCommand.h"

namespace wolkabout
{
class FirmwareUpdateResponse
{
public:
	enum class Status
	{
		OK,
		ERROR
	};

	enum class ErrorCode
	{
		UNSPECIFIED_ERROR = 0,
		FIRMWARE_UPDATE_DISABLED,
		FIRMWARE_FILE_NOT_DOWNLOADED
	};

	FirmwareUpdateResponse();
	FirmwareUpdateResponse(FirmwareUpdateResponse::Status status, FirmwareUpdateCommand::Type command);
	FirmwareUpdateResponse(FirmwareUpdateResponse::Status status, FirmwareUpdateCommand::Type command,
						   FirmwareUpdateResponse::ErrorCode errorCode);

	virtual ~FirmwareUpdateResponse() = default;

	FirmwareUpdateResponse::Status getStatus() const;
	FirmwareUpdateCommand::Type getCommand() const;
	WolkOptional<FirmwareUpdateResponse::ErrorCode> getErrorCode() const;

private:
	FirmwareUpdateResponse::Status m_status;
	FirmwareUpdateCommand::Type m_command;
	WolkOptional<FirmwareUpdateResponse::ErrorCode> m_errorCode;
};
}

#endif // FIRMWAREUPDATERESPONSE_H

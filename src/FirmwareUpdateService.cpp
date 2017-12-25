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


#ifndef FIRMWAREUPDATESERVICE_CPP
#define FIRMWAREUPDATESERVICE_CPP

#include "FirmwareUpdateService.h"
#include "model/FirmwareUpdateCommand.h"
#include "model/FirmwareUpdateResponse.h"
#include "OutboundDataService.h"
#include "FirmwareInstaller.h"

namespace wolkabout
{
FirmwareUpdateService::FirmwareUpdateService(std::shared_ptr<OutboundServiceDataHandler> outboundDataHandler,
											 std::weak_ptr<FirmwareInstaller> firmwareInstaller) :
	m_outboundDataHandler{std::move(outboundDataHandler)}, m_firmwareInstaller{firmwareInstaller},
	m_currentState{FirmwareUpdateService::State::IDLE}, m_firmwareFileName{""}, m_abortCallback{}
{
}

void FirmwareUpdateService::handleFirmwareUpdateCommand(const FirmwareUpdateCommand& firmwareUpdateCommand)
{
	switch (m_currentState)
	{
		case FirmwareUpdateService::State::IDLE:
		{
			switch (firmwareUpdateCommand.getType())
			{
				case FirmwareUpdateCommand::Type::INIT:
				{
					m_currentState = FirmwareUpdateService::State::FILE_DOWNLOAD;

					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::OK,
												   FirmwareUpdateCommand::Type::INIT};
					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				case FirmwareUpdateCommand::Type::INSTALL:
				{
					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::ERROR,
								FirmwareUpdateCommand::Type::INSTALL,
								FirmwareUpdateResponse::ErrorCode::FIRMWARE_FILE_NOT_DOWNLOADED};

					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				case FirmwareUpdateCommand::Type::ABORT:
				{
					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::ERROR,
								firmwareUpdateCommand.getType(),
								FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR};
					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				default:
					break;
			}

			break;
		}
		case FirmwareUpdateService::State::FILE_DOWNLOAD:
		{
			switch (firmwareUpdateCommand.getType())
			{
				case FirmwareUpdateCommand::Type::INIT:
				{
					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::ERROR,
								FirmwareUpdateCommand::Type::INIT,
								FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR};

					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				case FirmwareUpdateCommand::Type::INSTALL:
				{
					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::ERROR,
								FirmwareUpdateCommand::Type::INSTALL,
								FirmwareUpdateResponse::ErrorCode::FIRMWARE_FILE_NOT_DOWNLOADED};

					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				case FirmwareUpdateCommand::Type::ABORT:
				{
					m_currentState = FirmwareUpdateService::State::IDLE;

					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::OK,
												   FirmwareUpdateCommand::Type::ABORT};

					if(m_abortCallback)
					{
						m_abortCallback();
					}

					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				default:
					break;
			}

			break;
		}
		case FirmwareUpdateService::State::READY_FOR_INSTALL:
		{
			switch (firmwareUpdateCommand.getType())
			{
				case FirmwareUpdateCommand::Type::INIT:
				{
					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::ERROR,
								FirmwareUpdateCommand::Type::INIT,
								FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR};

					m_outboundDataHandler->addFirmwareUpdateResponse(response);
					break;
				}
				case FirmwareUpdateCommand::Type::INSTALL:
				{
					if(auto installer = m_firmwareInstaller.lock())
					{
						FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::OK,
													   FirmwareUpdateCommand::Type::INSTALL};
						m_outboundDataHandler->addFirmwareUpdateResponse(response);

						installer->install(m_firmwareFileName);
					}
					else
					{
						FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::ERROR,
									FirmwareUpdateCommand::Type::INSTALL,
									FirmwareUpdateResponse::ErrorCode::UNSPECIFIED_ERROR};

						m_outboundDataHandler->addFirmwareUpdateResponse(response);
					}

					m_currentState = FirmwareUpdateService::State::IDLE;

					break;
				}
				case FirmwareUpdateCommand::Type::ABORT:
				{
					m_currentState = FirmwareUpdateService::State::IDLE;

					FirmwareUpdateResponse response{FirmwareUpdateResponse::Status::OK,
												   FirmwareUpdateCommand::Type::ABORT};
					if(m_abortCallback)
					{
						m_abortCallback();
					}

					m_outboundDataHandler->addFirmwareUpdateResponse(response);

					break;
				}
				default:
					break;
			}

			break;
		}
		default:
			break;
	}
}

void FirmwareUpdateService::onFirmwareFileDownloaded(const std::string fileName)
{
	if(m_currentState == FirmwareUpdateService::State::FILE_DOWNLOAD)
	{
		m_currentState = FirmwareUpdateService::State::READY_FOR_INSTALL;
		m_firmwareFileName = fileName;
	}
}

void FirmwareUpdateService::setOnUpdateAbortCallback(std::function<void()> callback)
{
	m_abortCallback = callback;
}
}

#endif // FIRMWAREUPDATESERVICE_CPP

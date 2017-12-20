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

#include "FileDownloadMqttService.h"
#include "model/FileDownloadMqttCommand.h"
#include "model/FileDownloadMqttResponse.h"
#include "OutboundServiceDataHandler.h"
#include "FileHandler.h"

namespace
{
const std::string SERVICE_FILE_PATH = "";
const int MAX_PACKAGE_SIZE = 131072;
}

namespace wolkabout
{

FileDownloadMqttService::FileDownloadMqttService(std::shared_ptr<OutboundServiceDataHandler> outboundDataHandler) :
	m_currentState{FileDownloadMqttService::State::IDLE}, m_outboundDataHandler{std::move(outboundDataHandler)},
	m_fileHandler{new FileHandler(SERVICE_FILE_PATH, MAX_PACKAGE_SIZE)}
{
}

void FileDownloadMqttService::handleBinaryData(const BinaryData& binaryData)
{
	switch (m_currentState)
	{
		case FileDownloadMqttService::State::IDLE:
		{
			FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
											  FileDownloadMqttResponse::ErrorCode::FILE_TRANSFER_NOT_INITIATED);
			m_outboundDataHandler->addFileDownloadMqttResponse(response);

			break;
		}
		case FileDownloadMqttService::State::DOWNLOAD:
		{
			int packageNumber = -1;
			auto result = m_fileHandler->handleData(binaryData, packageNumber);

			switch (result)
			{
				case FileHandler::StatusCode::OK:
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::OK, packageNumber);
					m_outboundDataHandler->addFileDownloadMqttResponse(response);
					break;
				}
				case FileHandler::StatusCode::PACKAGE_HASH_NOT_VALID:
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::PACKAGE_HASH_INVALID,
													  packageNumber);
					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}
				case FileHandler::StatusCode::PREVIOUS_PACKAGE_HASH_NOT_VALID:
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::PREVIOUS_PACKAGE_HASH_INVALID,
													  packageNumber);
					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}
				default:
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);
					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}
			}

			break;
		}
		default:
			break;
	}
}

void FileDownloadMqttService::handleFileDownloadMqttCommand(const FileDownloadMqttCommand& fileDownloadMqttCommand)
{
	switch (m_currentState)
	{
		case FileDownloadMqttService::State::IDLE:
		{
			if(fileDownloadMqttCommand.getType() == FileDownloadMqttCommand::Type::INIT)
			{
				auto packageSize = fileDownloadMqttCommand.getSize();
				if(packageSize.null() || static_cast<int>(packageSize) <= 0)
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}

				auto fileName = fileDownloadMqttCommand.getName();
				if(fileName.null() || static_cast<std::string>(fileName).empty())
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}

				auto packageCount = fileDownloadMqttCommand.getCount();
				if(packageCount.null() || static_cast<int>(packageCount) <= 0)
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}

				auto fileHash = fileDownloadMqttCommand.getHash();
				if(fileHash.null() || static_cast<std::string>(fileHash).empty())
				{
					FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
													  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

					m_outboundDataHandler->addFileDownloadMqttResponse(response);

					break;
				}

				auto result = m_fileHandler->prepare(fileName, packageSize, packageCount, fileHash);

				switch (result)
				{
					case FileHandler::StatusCode::OK:
					{
						m_fileName = fileName;

						FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::OK);
						m_outboundDataHandler->addFileDownloadMqttResponse(response);

						m_currentState = FileDownloadMqttService::State::DOWNLOAD;

						break;
					}
					case FileHandler::StatusCode::PACKAGE_SIZE_NOT_SUPPORTED:
					{
						FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
														  FileDownloadMqttResponse::ErrorCode::PACKAGE_LENGTH_NOT_SUPPORTED);
						m_outboundDataHandler->addFileDownloadMqttResponse(response);

						break;
					}
					default:
					{
						FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
														  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

						m_outboundDataHandler->addFileDownloadMqttResponse(response);

						break;
					}
				}
			}
			else if (fileDownloadMqttCommand.getType() == FileDownloadMqttCommand::Type::END)
			{
				FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::OK);

				m_outboundDataHandler->addFileDownloadMqttResponse(response);
			}

			break;
		}
		case FileDownloadMqttService::State::DOWNLOAD:
		{
			if (fileDownloadMqttCommand.getType() == FileDownloadMqttCommand::Type::END)
			{
				auto result = m_fileHandler->validateFile();

				switch (result)
				{
					case FileHandler::StatusCode::OK:
					{
						result = m_fileHandler->saveFile();

						switch (result)
						{
							case FileHandler::StatusCode::OK:
							{
								FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::OK);

								m_outboundDataHandler->addFileDownloadMqttResponse(response);

								if(m_fileDownloadedCallback)
								{
									m_fileDownloadedCallback(m_fileName);
								}

								break;
							}
							case FileHandler::StatusCode::FILE_HANDLING_ERROR:
							{
								FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
																  FileDownloadMqttResponse::ErrorCode::FILE_HANDLING_ERROR);

								m_outboundDataHandler->addFileDownloadMqttResponse(response);

								break;
							}
							default:
							{
								FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
																  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

								m_outboundDataHandler->addFileDownloadMqttResponse(response);

								break;
							}
						}

						break;
					}
					case FileHandler::StatusCode::TRANSFER_NOT_COMPLETED:
					{
						FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
														  FileDownloadMqttResponse::ErrorCode::NOT_ALL_PACKAGES_RECEIVED);

						m_outboundDataHandler->addFileDownloadMqttResponse(response);

						break;
					}
					case FileHandler::StatusCode::FILE_HASH_NOT_VALID:
					{
						FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
														  FileDownloadMqttResponse::ErrorCode::FILE_CHECKSUM_INVALID);

						m_outboundDataHandler->addFileDownloadMqttResponse(response);

						break;
					}
					default:
					{
						FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
														  FileDownloadMqttResponse::ErrorCode::UNSPECIFIED_ERROR);

						m_outboundDataHandler->addFileDownloadMqttResponse(response);

						break;
					}
				}

				m_currentState = FileDownloadMqttService::State::IDLE;
				m_fileHandler->clear();
			}
			else if (fileDownloadMqttCommand.getType() == FileDownloadMqttCommand::Type::INIT)
			{
				FileDownloadMqttResponse response(FileDownloadMqttResponse::Status::ERROR,
												  FileDownloadMqttResponse::ErrorCode::FILE_TRANSFER_IN_PROGRESS);

				m_outboundDataHandler->addFileDownloadMqttResponse(response);
			}

			break;
		}
		default:
			break;
	}
}

void FileDownloadMqttService::setFileDownloadedCallback(std::function<void(const std::string&)> callback)
{
	m_fileDownloadedCallback = callback;
}

}

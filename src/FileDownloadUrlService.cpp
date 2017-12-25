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

#include "FileDownloadUrlService.h"
#include "model/FileDownloadUrlCommand.h"
#include "model/FileDownloadUrlResponse.h"
#include "OutboundServiceDataHandler.h"
#include "UrlFileDownloader.h"

namespace wolkabout
{
FileDownloadUrlService::FileDownloadUrlService(std::shared_ptr<OutboundServiceDataHandler> outboundDataHandler,
											   std::weak_ptr<UrlFileDownloader> downloader) :
	m_outboundDataHandler{std::move(outboundDataHandler)}, m_urlFileDownloader{downloader},
	m_currentState{FileDownloadUrlService::State::IDLE}, m_file{}
{
	if(auto downloader = m_urlFileDownloader.lock())
	{
		downloader->setOnSuccessCallback([this](const std::string& file){
			FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::COMPLETED,
						FileDownloadUrlCommand::Type::UPLOAD};

			m_outboundDataHandler->addFileDownloadUrlResponse(response);

			m_file = file;
			m_currentState = FileDownloadUrlService::State::COMPLETED;
		});

		downloader->setOnErrorCallback([this](){
			FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
						FileDownloadUrlCommand::Type::UPLOAD,
						FileDownloadUrlResponse::StatusCode::FILE_HANDLING_ERROR};

			m_outboundDataHandler->addFileDownloadUrlResponse(response);

			m_currentState = FileDownloadUrlService::State::ERROR;
		});
	}
}

void FileDownloadUrlService::handleFileDownloadUrlCommand(const FileDownloadUrlCommand& fileDownloadUrlCommand)
{
	switch (m_currentState)
	{
		case FileDownloadUrlService::State::IDLE:
		{
			switch (fileDownloadUrlCommand.getType())
			{
				case FileDownloadUrlCommand::Type::INIT:
				{
					auto url = fileDownloadUrlCommand.getUrl();

					if(url.null() || static_cast<std::string>(url).empty())
					{
						FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
									FileDownloadUrlCommand::Type::INIT,
									FileDownloadUrlResponse::StatusCode::UNSPECIFIED_ERROR};

						m_outboundDataHandler->addFileDownloadUrlResponse(response);

						break;
					}

					if(auto downloader = m_urlFileDownloader.lock())
					{
						FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::OK,
									FileDownloadUrlCommand::Type::INIT};

						m_outboundDataHandler->addFileDownloadUrlResponse(response);

						m_currentState = FileDownloadUrlService::State::DOWNLOAD;

						downloader->download(url);
					}
					else
					{
						FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
									FileDownloadUrlCommand::Type::INIT,
									FileDownloadUrlResponse::StatusCode::UNSPECIFIED_ERROR};

						m_outboundDataHandler->addFileDownloadUrlResponse(response);
					}

					break;
				}
				case FileDownloadUrlCommand::Type::END:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::END,
								FileDownloadUrlResponse::StatusCode::DOWNLOAD_NOT_INITIATED};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::STATUS:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::OK,
								FileDownloadUrlCommand::Type::STATUS};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::UPLOAD:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::UPLOAD,
								FileDownloadUrlResponse::StatusCode::UNSPECIFIED_ERROR};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				default:
					break;
			}

			break;
		}
		case FileDownloadUrlService::State::DOWNLOAD:
		{
			switch (fileDownloadUrlCommand.getType())
			{
				case FileDownloadUrlCommand::Type::INIT:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::INIT,
								FileDownloadUrlResponse::StatusCode::DOWNLOAD_ALREADY_STARTED};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::END:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::END,
								FileDownloadUrlResponse::StatusCode::DOWNLOAD_NOT_COMPLETED};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					if(auto downloader = m_urlFileDownloader.lock())
					{
						downloader->abort();
					}

					m_currentState = FileDownloadUrlService::State::IDLE;

					break;
				}
				case FileDownloadUrlCommand::Type::STATUS:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::IN_PROGRESS,
								FileDownloadUrlCommand::Type::STATUS};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::UPLOAD:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::UPLOAD,
								FileDownloadUrlResponse::StatusCode::UNSPECIFIED_ERROR};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				default:
					break;
			}

			break;
		}
		case FileDownloadUrlService::State::COMPLETED:
		{
			switch (fileDownloadUrlCommand.getType())
			{
				case FileDownloadUrlCommand::Type::INIT:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::INIT,
								FileDownloadUrlResponse::StatusCode::DOWNLOAD_ALREADY_STARTED};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::END:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::OK,
								FileDownloadUrlCommand::Type::END};
					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					if(m_fileDownloadedCallback)
					{
						m_fileDownloadedCallback(m_file);
					}

					m_currentState = FileDownloadUrlService::State::IDLE;

					break;
				}
				case FileDownloadUrlCommand::Type::STATUS:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::COMPLETED,
								FileDownloadUrlCommand::Type::STATUS};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::UPLOAD:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::UPLOAD,
								FileDownloadUrlResponse::StatusCode::UNSPECIFIED_ERROR};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				default:
					break;
			}

			break;
		}
		case FileDownloadUrlService::State::ERROR:
		{
			switch (fileDownloadUrlCommand.getType())
			{
				case FileDownloadUrlCommand::Type::INIT:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::INIT,
								FileDownloadUrlResponse::StatusCode::DOWNLOAD_ALREADY_STARTED};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::END:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::END,
								FileDownloadUrlResponse::StatusCode::FILE_HANDLING_ERROR};
					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					m_currentState = FileDownloadUrlService::State::IDLE;

					break;
				}
				case FileDownloadUrlCommand::Type::STATUS:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::STATUS,
								FileDownloadUrlResponse::StatusCode::FILE_HANDLING_ERROR};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

					break;
				}
				case FileDownloadUrlCommand::Type::UPLOAD:
				{
					FileDownloadUrlResponse response{FileDownloadUrlResponse::Status::ERROR,
								FileDownloadUrlCommand::Type::UPLOAD,
								FileDownloadUrlResponse::StatusCode::UNSPECIFIED_ERROR};

					m_outboundDataHandler->addFileDownloadUrlResponse(response);

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

void FileDownloadUrlService::setFileDownloadedCallback(std::function<void(const std::string&)> callback)
{
	m_fileDownloadedCallback = callback;
}

void FileDownloadUrlService::abortDownload()
{
	if(m_currentState != FileDownloadUrlService::State::IDLE)
	{
		if(auto downloader = m_urlFileDownloader.lock())
		{
			downloader->abort();
		}

		m_currentState = FileDownloadUrlService::State::IDLE;
		m_file = "";
	}
}
}

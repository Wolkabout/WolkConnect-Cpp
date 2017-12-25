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

#ifndef FILEDOWNLOADURLSERVICE_H
#define FILEDOWNLOADURLSERVICE_H

#include "CommandHandlingService.h"
#include "FileDownloadUrlCommandListener.h"
#include <functional>
#include <memory>

namespace wolkabout
{
class OutboundServiceDataHandler;
class UrlFileDownloader;

class FileDownloadUrlService: public CommandHandlingService, public FileDownloadUrlCommandListener
{
public:
	FileDownloadUrlService(std::shared_ptr<OutboundServiceDataHandler> outboundDataHandler,
						   std::weak_ptr<UrlFileDownloader> downloader);

	void handleFileDownloadUrlCommand(const FileDownloadUrlCommand& fileDownloadUrlCommand) override;

	void setFileDownloadedCallback(std::function<void(const std::string&)> callback);

	void abortDownload();

private:
	enum class State
	{
		IDLE,
		DOWNLOAD,
		COMPLETED,
		ERROR
	};

	std::shared_ptr<OutboundServiceDataHandler> m_outboundDataHandler;
	std::weak_ptr<UrlFileDownloader> m_urlFileDownloader;
	std::function<void(const std::string&)> m_fileDownloadedCallback;

	FileDownloadUrlService::State m_currentState;
	std::string m_file;
};
}

#endif // FILEDOWNLOADURLSERVICE_H

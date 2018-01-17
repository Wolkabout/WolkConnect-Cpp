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

#ifndef FILEDOWNLOADSERVICE_H
#define FILEDOWNLOADSERVICE_H

#include "CommandHandlingService.h"
#include "WolkaboutFileDownloader.h"
#include "BinaryDataListener.h"
#include "CommandBuffer.h"
#include "utilities/ByteUtils.h"
#include "utilities/Timer.h"
#include <stdint.h>
#include <memory>

namespace wolkabout
{
class OutboundServiceDataHandler;
class FileHandler;

class FileDownloadService: public CommandHandlingService, public WolkaboutFileDownloader, public BinaryDataListener
{
public:
	FileDownloadService(uint_fast64_t maxFileSize, uint_fast64_t maxPacketSize, std::unique_ptr<FileHandler> fileHandler,
						std::shared_ptr<OutboundServiceDataHandler> outboundDataHandler);

	void download(const std::string& fileName, uint_fast64_t fileSize, const ByteArray& fileHash,
				  const std::string& downloadDirectory, std::function<void(const std::string& filePath)> onSuccessCallback,
				  std::function<void(WolkaboutFileDownloader::Error errorCode)> onFailCallback) override;

	void abort() override;

	void handleBinaryData(const BinaryData& binaryData) override;

private:
	void addToCommandBuffer(std::function<void()> command);

	void requestPacket(unsigned index, uint_fast64_t size);

	void packetFailed();

	void clear();

	class FileDownloadServiceState
	{
	public:
		FileDownloadServiceState(FileDownloadService& service) : m_service{service} {}
		virtual ~FileDownloadServiceState() = default;
		virtual void handleBinaryData(const BinaryData& binaryData) = 0;
		virtual void download(const std::string& fileName, uint_fast64_t fileSize, const ByteArray& fileHash,
							  const std::string& downloadDirectory, std::function<void(const std::string& filePath)> onSuccessCallback,
							  std::function<void(WolkaboutFileDownloader::Error errorCode)> onFailCallback) = 0;
		virtual void abort() = 0;

	protected:
		FileDownloadService& m_service;
	};

	class IdleState: public FileDownloadServiceState
	{
	public:
		using FileDownloadServiceState::FileDownloadServiceState;
		void handleBinaryData(const BinaryData& binaryData) override;
		void download(const std::string& fileName, uint_fast64_t fileSize, const ByteArray& fileHash,
					  const std::string& downloadDirectory, std::function<void(const std::string& filePath)> onSuccessCallback,
					  std::function<void(WolkaboutFileDownloader::Error errorCode)> onFailCallback) override;
		void abort() override;
	};

	class DownloadState: public FileDownloadServiceState
	{
	public:
		using FileDownloadServiceState::FileDownloadServiceState;
		void handleBinaryData(const BinaryData& binaryData) override;
		void download(const std::string& fileName, uint_fast64_t fileSize, const ByteArray& fileHash,
					  const std::string& downloadDirectory, std::function<void(const std::string& filePath)> onSuccessCallback,
					  std::function<void(WolkaboutFileDownloader::Error errorCode)> onFailCallback) override;
		void abort() override;
	};

	friend class IdleState;
	friend class DownloadState;

	uint_fast64_t m_maxFileSize;
	uint_fast64_t m_maxPacketSize;
	std::unique_ptr<FileHandler> m_fileHandler;
	std::shared_ptr<OutboundServiceDataHandler> m_outboundDataHandler;

	std::unique_ptr<CommandBuffer> m_commandBuffer;

	std::unique_ptr<IdleState> m_idleState;
	std::unique_ptr<DownloadState> m_downloadState;
	FileDownloadServiceState* m_currentState;

	Timer m_timer;

	std::string m_currentFileName;
	uint_fast64_t m_currentFileSize;
	uint_fast64_t m_currentPacketSize;
	unsigned m_currentPacketCount;
	unsigned m_currentPacketIndex;
	ByteArray m_currentFileHash;
	std::string m_currentDownloadDirectory;
	std::function<void(const std::string&)> m_currentOnSuccessCallback;
	std::function<void(WolkaboutFileDownloader::Error)> m_currentOnFailCallback;

	static const unsigned short MAX_RETRY_COUNT = 3;
	unsigned short m_retryCount;

	static const unsigned short PACKET_REQUEST_TIMEOUT_MSEC = 60000;
};
}

#endif // FILEDOWNLOADSERVICE_H

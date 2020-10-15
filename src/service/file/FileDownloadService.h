/*
 * Copyright 2018 WolkAbout Technology s.r.o.
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

#include "InboundMessageHandler.h"
#include "model/FileTransferStatus.h"
#include "utilities/CommandBuffer.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

namespace wolkabout
{
class BinaryData;
class FileDelete;
class FileDownloader;
class FilePacketRequest;
class FileRepository;
class FileUploadAbort;
class FileUploadInitiate;
class FileUploadStatus;
class FileUrlDownloadAbort;
class FileUrlDownloadInitiate;
class FileUrlDownloadStatus;
class JsonDownloadProtocol;
class ConnectivityService;
class UrlFileDownloader;

class FileDownloadService : public MessageListener
{
public:
    FileDownloadService(std::string deviceKey, JsonDownloadProtocol& protocol, std::string fileDownloadDirectory,
                        std::uint64_t maxPacketSize, ConnectivityService& connectivityService,
                        FileRepository& fileRepository, std::shared_ptr<UrlFileDownloader> urlFileDownloader = nullptr);

    ~FileDownloadService();

    FileDownloadService(const FileDownloadService&) = delete;
    FileDownloadService& operator=(const FileDownloadService&) = delete;

    virtual void messageReceived(std::shared_ptr<Message> message) override;

    virtual const Protocol& getProtocol() override;

    virtual void sendFileList();

private:
    void handle(const BinaryData& binaryData);
    void handle(const FileUploadInitiate& request);
    void handle(const FileUploadAbort& request);
    void handle(const FileDelete& request);
    void handle(const FileUrlDownloadInitiate& request);
    void handle(const FileUrlDownloadAbort& request);

    void download(const std::string& fileName, std::uint64_t fileSize, const std::string& fileHash);
    void urlDownload(const std::string& fileUrl);
    void abortDownload(const std::string& fileName);
    void abortUrlDownload(const std::string& fileUrl);
    void deleteFile(const std::string& fileName);
    void purgeFiles();

    void sendStatus(const FileUploadStatus& response);
    void sendStatus(const FileUrlDownloadStatus& response);
    void sendFileListUpdate();
    void sendFileListResponse();

    void requestPacket(const FilePacketRequest& request);
    void downloadCompleted(const std::string& fileName, const std::string& filePath, const std::string& fileHash);
    void downloadFailed(const std::string& fileName, FileTransferError errorCode);

    void urlDownloadCompleted(const std::string& fileUrl, const std::string& fileName, const std::string& filePath);
    void urlDownloadFailed(const std::string& fileUrl, FileTransferError errorCode);

    void addToCommandBuffer(std::function<void()> command);

    void flagCompletedDownload(const std::string& key);
    void clearDownloads();
    void notifyCleanup();

    const std::string m_deviceKey;
    const std::string m_fileDownloadDirectory;
    const std::uint64_t m_maxPacketSize;

    JsonDownloadProtocol& m_protocol;

    ConnectivityService& m_connectivityService;
    FileRepository& m_fileRepository;

    std::shared_ptr<UrlFileDownloader> m_urlFileDownloader;

    // temporary to disallow simultaneous downloads
    std::string m_activeDownload;
    std::map<std::string, std::tuple<std::string, std::unique_ptr<FileDownloader>, bool>> m_activeDownloads;

    std::atomic_bool m_run;
    std::condition_variable m_condition;
    std::recursive_mutex m_mutex;
    std::thread m_garbageCollector;

    CommandBuffer m_commandBuffer;
};
}    // namespace wolkabout

#endif    // FILEDOWNLOADSERVICE_H

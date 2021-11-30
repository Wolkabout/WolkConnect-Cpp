/**
 * Copyright 2021 Wolkabout s.r.o.
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

#ifndef WOLKABOUTCONNECTOR_POCOFILEDOWNLOADER_H
#define WOLKABOUTCONNECTOR_POCOFILEDOWNLOADER_H

#include "core/utilities/CommandBuffer.h"
#include "wolk/service/file_management/FileDownloader.h"

#include <memory>
#include <thread>
#include <core/utilities/ByteUtils.h>

namespace Poco
{
namespace Net
{
    class HTTPClientSession;
}
}    // namespace Poco

namespace wolkabout
{
class PocoFileDownloader : public FileDownloader
{
public:
    /**
     * Default constructor.
     */
    explicit PocoFileDownloader(CommandBuffer& commandBuffer);

    /**
     * Overridden destructor. Will abort the download and stop the thread.
     */
    ~PocoFileDownloader() override;

    /**
     * Overridden method from the `FileDownloader` interface.
     * This is the getter for the current status of the download.
     *
     * @return The downloading status.
     */
    FileUploadStatus getStatus() override;

    /**
     * Overridden method from the `FileDownloader` interface.
     * This is the getter for the name of the downloaded file.
     * The downloader will decide on the name once it has been successfully downloaded.
     *
     * @return The decided name of the new file.
     */
    std::string getName() override;

    /**
     * Overridden method from the `FileDownloader` interface.
     * This is the getter for all the bytes that have been obtained for the file.
     *
     * @return Vector containing all bytes once the file has been successfully downloaded.
     */
    ByteArray getBytes() override;

    /**
     * Overridden method from the `FileDownloader` interface that allows the user to initiate the download of the file.
     *
     * @param url The URL from which the file should be downloaded.
     * @param statusCallback The status callback that should be used to notify the sender of new status changes.
     */
    void downloadFile(const std::string& url,
                      std::function<void(FileUploadStatus, FileUploadError, std::string)> statusCallback) override;

    /**
     * Overridden method from the `FileDownloader` interface that allows the user to abort the downloading of the file.
     */
    void abortDownload() override;

private:
    /**
     * This is the internal method that will be invoked in the other thread to download the file.
     *
     * @param url The url from which the downloader needs to download a file.
     */
    void download(const std::string& url);

    /**
     * This is the internal routine of how the connection is stopped.
     */
    void stop();

    /**
     * This is an internal method that will queue the external task of announcing the status/error/fileName.
     *
     * @param status The new status value.
     * @param error The new error value.
     * @param fileName The new file name value.
     */
    void changeStatus(FileUploadStatus status, FileUploadError error = FileUploadError::NONE,
                      const std::string& fileName = "");

    /**
     * This is the utility method used by startConnection to extract the host path from the client target path passed to
     * the object.
     *
     * @param targetPath The target path that was given to target the other side of the connection.
     * @return The host that can be given to the HTTP client session.
     */
    static std::string extractHost(std::string targetPath);

    /**
     * This is the utility method used by startConnection to extract the host port from the client target path passed to
     * the object.
     *
     * @param targetPath The target path that was given to target the other side of the connection.
     * @return The port that can be given to the HTTP client session.
     */
    static std::uint16_t extractPort(std::string targetPath);

    /**
     * This is the utility method used by startConnection to extract the host uri from the client target path passed to
     * the object.
     *
     * @param targetPath The target path that was given to target the other side of the connection.
     * @return The url that can be given to the HTTP request.
     */
    static std::string extractUri(std::string targetPath);

    // Here is everything regarding the status
    std::mutex m_mutex;
    FileUploadStatus m_status;
    std::string m_name;
    ByteArray m_bytes;
    std::function<void(FileUploadStatus, FileUploadError, std::string)> m_statusCallback;
    CommandBuffer& m_commandBuffer;

    // Here we store the session so the session can be closed in case of abort
    std::unique_ptr<Poco::Net::HTTPClientSession> m_session;
    std::unique_ptr<std::thread> m_thread;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_POCOFILEDOWNLOADER_H

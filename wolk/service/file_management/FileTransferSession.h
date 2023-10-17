/**
 * Copyright 2022 Wolkabout Technology s.r.o.
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

#ifndef WOLKABOUTCONNECTOR_FILETRANSFERSESSION_H
#define WOLKABOUTCONNECTOR_FILETRANSFERSESSION_H

#include "core/utility/ByteUtils.h"
#include "core/utility/CommandBuffer.h"
#include "wolk/service/file_management/FileDownloader.h"

#include <memory>
#include <string>

namespace wolkabout
{
// Forward declaring some messages from the SDK
class FileBinaryRequestMessage;
class FileBinaryResponseMessage;
class FileUploadInitiateMessage;
class FileUrlDownloadInitMessage;

namespace connect
{
/**
 * This structure represents a single chunk that is always received in exactly one `FileBinaryResponse` message.
 */
struct FileChunk
{
    std::string previousHash;
    legacy::ByteArray bytes;
    std::string hash;
};

/**
 * This class represents a single session of file transfer. It can be either a file upload session, or a file url
 * download session. Based on that, the session will either collect FileChunk, or host a FileDownloader.
 */
class FileTransferSession
{
public:
    /**
     * Default constructor for the FileTransferSession in case of a regular file transfer.
     *
     * @param deviceKey The device key for which this session is going on.
     * @param message The message that initiated an upload.
     * @param callback The callback that the session should use to announce status and error changes.
     * @param commandBuffer The command buffer which the session will use to announce status.
     */
    FileTransferSession(std::string deviceKey, const FileUploadInitiateMessage& message,
                        std::function<void(FileTransferStatus, FileTransferError)> callback,
                        legacy::CommandBuffer& commandBuffer);

    /**
     * Default constructor for the FileTransferSession in case of a url download transfer.
     *
     * @param deviceKey The device key for which this session is going on.
     * @param message The message that initiated an upload.
     * @param callback The callback that the session should use to announce status and error changes.
     * @param commandBuffer The command buffer which the session will use to announce status.
     * @param fileDownloader The file downloader that will actually execute the file download.
     */
    FileTransferSession(std::string deviceKey, const FileUrlDownloadInitMessage& message,
                        std::function<void(FileTransferStatus, FileTransferError)> callback,
                        legacy::CommandBuffer& commandBuffer, std::shared_ptr<FileDownloader> fileDownloader);

    /**
     * Default virtual destructor.
     */
    virtual ~FileTransferSession() = default;

    /**
     * Default getter for the information if the session is a platform transfer session.
     *
     * @return Is this session a platform transfer session.
     */
    virtual bool isPlatformTransfer() const;

    /**
     * Default getter for the information if the session is a url download session.
     *
     * @return Is this session a url download session.
     */
    virtual bool isUrlDownload() const;

    /**
     * Default getter for the information if the session is done.
     *
     * @return Is this session done?
     */
    virtual bool isDone() const;

    /**
     * Default getter for the device key.
     *
     * @return The device key for which this session is going on.
     */
    virtual const std::string& getDeviceKey() const;

    /**
     * Default getter for the name of the file.
     * This might change throughout the session, as URL downloads might obtain the name later.
     *
     * @return The name of the file being obtained through this session.
     */
    virtual const std::string& getName() const;

    /**
     * Default getter for the URL of the file.
     * The URL from which the file will be obtained. If it is not set, the file is not obtained through a URL.
     *
     * @return The URL from which a file will be obtained.
     */
    virtual const std::string& getUrl() const;

    /**
     * This is a method that allows the user to abort the session.
     */
    virtual void abort();

    /**
     * This is a method that will attempt to create a chunk out of a FileBinaryResponse message.
     *
     * @param message Binary response containing bytes and hashes.
     * @return The current status of the session.
     */
    virtual FileTransferError pushChunk(const FileBinaryResponseMessage& message);

    /**
     * This is a method that will hand out the next FileBinaryRequest if the session is in a transfer mode, and in
     * `FILE_TRANSFER` status.
     *
     * @return The next request message that should be sent out. If the name is empty, that's an invalid request.
     */
    virtual FileBinaryRequestMessage getNextChunkRequest();

    /**
     * This is a method that will start the download of a file.
     *
     * @return If the trigger was successful.
     */
    virtual bool triggerDownload();

    /**
     * Default getter for the current status of the transfer session.
     *
     * @return The current status of the transfer.
     */
    virtual FileTransferStatus getStatus() const;

    /**
     * Default getter for the error of the transfer session.
     *
     * @return The current error of the transfer. Could be `NONE`.
     */
    virtual FileTransferError getError() const;

    /**
     * Default getter for the chunks that the session has collected.
     *
     * @return The vector containing all the chunks the session has collected.
     */
    virtual const std::vector<FileChunk>& getChunks() const;

private:
    /**
     * This is an internal method that is used to change the internal status and error, and announce them over the
     * callback to whoever listens to the status and error.
     *
     * @param status The new status value.
     * @param error The new error value.
     */
    void changeStatusAndError(FileTransferStatus status, FileTransferError error);

    // Here are the parameters for engaging the session.
    // The device for which the session is ongoing
    std::string m_deviceKey;

    // If the URL is set, it is always a URL download session.
    std::string m_name;
    std::string m_url;

    // Here we store the information whether the session is done
    std::uint64_t m_retryCount;
    std::atomic_bool m_done;

    // If the session is meant to be a file upload session, it should hold chunks.
    // And it should also use the size/hash declared by the platform
    std::uint64_t m_size;
    std::string m_hash;
    std::vector<FileChunk> m_chunks;

    // If the session is meant to be a file url download session, it should hold a file downloader.
    std::shared_ptr<FileDownloader> m_downloader;

    // Here is the place for the status and the error, and the callback
    std::mutex m_mutex;
    FileTransferStatus m_status;
    FileTransferError m_error;
    std::function<void(FileTransferStatus, FileTransferError)> m_callback;
    legacy::CommandBuffer& m_commandBuffer;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILETRANSFERSESSION_H

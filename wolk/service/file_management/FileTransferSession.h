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

#ifndef WOLKABOUTCONNECTOR_FILETRANSFERSESSION_H
#define WOLKABOUTCONNECTOR_FILETRANSFERSESSION_H

#include "core/utilities/ByteUtils.h"
#include "core/utilities/CommandBuffer.h"
#include "wolk/service/file_management/FileDownloader.h"

#include <memory>
#include <string>

namespace wolkabout
{
class FileBinaryRequestMessage;
class FileBinaryResponseMessage;
class FileUploadInitiateMessage;
class FileUrlDownloadInitMessage;

/**
 * This structure represents a single chunk that is always received in exactly one `FileBinaryResponse` message.
 */
struct FileChunk
{
    std::string previousHash;
    ByteArray bytes;
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
     * @param message The message that initiated an upload.
     * @param callback The callback that the session should use to announce status and error changes.
     * @param maxSize The max size of a message chunk that needs to be sent out.
     */
    FileTransferSession(const FileUploadInitiateMessage& message,
                        std::function<void(FileUploadStatus, FileUploadError)> callback,
                        std::uint64_t maxSize = 268435455);

    /**
     * Default constructor for the FileTransferSession in case of a url download transfer.
     *
     * @param message The message that initiated an upload.
     * @param callback The callback that the session should use to announce status and error changes.
     */
    FileTransferSession(const FileUrlDownloadInitMessage& message,
                        std::function<void(FileUploadStatus, FileUploadError)> callback);

    /**
     * Default getter for the information if the session is a platform transfer session.
     *
     * @return Is this session a platform transfer session.
     */
    bool isPlatformTransfer() const;

    /**
     * Default getter for the information if the session is a url download session.
     *
     * @return Is this session a url download session.
     */
    bool isUrlDownload() const;

    /**
     * Default getter for the name of the file.
     * This might change throughout the session, as URL downloads might obtain the name later.
     *
     * @return The name of the file being obtained through this session.
     */
    const std::string& getName() const;

    /**
     * Default getter for the URL of the file.
     * The URL from which the file will be obtained. If it is not set, the file is not obtained through a URL.
     *
     * @return The URL from which a file will be obtained.
     */
    const std::string& getUrl() const;

    /**
     * This is a method that will attempt to create a chunk out of a FileBinaryResponse message.
     *
     * @param message Binary response containing bytes and hashes.
     * @return The current status of the session.
     */
    FileUploadStatus pushChunk(const FileBinaryResponseMessage& message);

    /**
     * This is a method that will hand out the next FileBinaryRequest if the session is in a transfer mode, and in
     * `FILE_TRANSFER` status.
     *
     * @return The next request message that should be sent out. If the name is empty, that's an invalid request.
     */
    FileBinaryRequestMessage getNextChunkRequest();

    /**
     * Default getter for the current status of the transfer session.
     *
     * @return The current status of the transfer.
     */
    FileUploadStatus getStatus() const;

    /**
     * Default getter for the error of the transfer session.
     *
     * @return The current error of the transfer. Could be `NONE`.
     */
    FileUploadError getError() const;

private:
    // Here are the parameters for engaging the session.
    // If the URL is set, it is always a URL download session.
    std::string m_name;
    std::string m_url;
    std::uint64_t m_maxSize;

    // If the session is meant to be a file upload session, it should hold chunks.
    // And it should also use the size/hash declared by the platform
    std::uint64_t m_size;
    std::string m_hash;
    std::vector<FileChunk> m_chunks;

    // If the session is meant to be a file url download session, it should hold a file downloader.
    std::shared_ptr<FileDownloader> m_downloader;

    // Here is the place for the status and the error, and the callback
    FileUploadStatus m_status;
    FileUploadError m_error;
    std::function<void(FileUploadStatus, FileUploadError)> m_callback;
    CommandBuffer m_commandBuffer;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILETRANSFERSESSION_H

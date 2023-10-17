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

#ifndef WOLKABOUTCONNECTOR_FILEDOWNLOADER_H
#define WOLKABOUTCONNECTOR_FILEDOWNLOADER_H

#include "core/Types.h"
#include "core/utility/ByteUtils.h"

#include <functional>
#include <string>

namespace wolkabout
{
namespace connect
{
/**
 * This interface represents an object that is capable of downloading a file, using the given url.
 */
class FileDownloader
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~FileDownloader() = default;

    /**
     * This is the getter by which the user can get the current status of the file download.
     *
     * @return The file download status.
     */
    virtual FileTransferStatus getStatus() const = 0;

    /**
     * This is the getter by which the user can get the file name of the downloaded file.
     *
     * @return The name of the downloaded file.
     */
    virtual const std::string& getName() const = 0;

    /**
     * This is the getter by which the user can get file bytes when the file has been downloaded.
     *
     * @return The byte array containing all bytes of the downloaded file.
     */
    virtual const legacy::ByteArray& getBytes() const = 0;

    /**
     * This is the method by which the FileManagementService will notify the downloader it should start downloading a
     * file.
     *
     * @param url The url from which a file should be downloaded.
     * @param statusCallback The callback by which the downloader should report status updates and or name changes.
     */
    virtual void downloadFile(
      const std::string& url,
      std::function<void(FileTransferStatus, FileTransferError, std::string)> statusCallback) = 0;

    /**
     * This is the method by which the FileManagementService will notify the downloader to try and attempt to abort the
     * download.
     */
    virtual void abortDownload() = 0;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILEDOWNLOADER_H

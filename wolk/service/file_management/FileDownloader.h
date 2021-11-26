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

#ifndef WOLKABOUTCONNECTOR_FILEDOWNLOADER_H
#define WOLKABOUTCONNECTOR_FILEDOWNLOADER_H

#include "core/Types.h"

#include <functional>
#include <string>

namespace wolkabout
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
    virtual ~FileDownloader() = 0;

    /**
     * This is the method by which the FileManagementService will notify the downloader it should start downloading a
     * file.
     *
     * @param url The url from which a file should be downloaded.
     * @param statusCallback The callback by which the downloader should report status updates and or name changes.
     */
    virtual void downloadFile(const std::string& url,
                              std::function<void(FileUploadStatus, FileUploadError, std::string)> statusCallback) = 0;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILEDOWNLOADER_H

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

#ifndef WOLKABOUTCONNECTOR_FILEDOWNLOADERMOCK_H
#define WOLKABOUTCONNECTOR_FILEDOWNLOADERMOCK_H

#include "wolk/service/file_management/FileDownloader.h"

#include <gmock/gmock.h>

using namespace wolkabout;
using namespace wolkabout::connect;

class FileDownloaderMock : public FileDownloader
{
public:
    MOCK_METHOD(FileTransferStatus, getStatus, (), (const));
    MOCK_METHOD(const std::string&, getName, (), (const));
    MOCK_METHOD(const ByteArray&, getBytes, (), (const));
    MOCK_METHOD(void, downloadFile,
                (const std::string&, std::function<void(FileTransferStatus, FileTransferError, std::string)>));
    MOCK_METHOD(void, abortDownload, ());
};

#endif    // WOLKABOUTCONNECTOR_FILEDOWNLOADERMOCK_H

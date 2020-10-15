
/*
 * Copyright 2020 WolkAbout Technology s.r.o.
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
#ifndef WOLKABOUTCONNECTOR_FILEDOWNLOADSERVICEMOCK_H
#define WOLKABOUTCONNECTOR_FILEDOWNLOADSERVICEMOCK_H

#include "service/file/FileDownloadService.h"

#include <gmock/gmock.h>

class FileDownloadServiceMock: public wolkabout::FileDownloadService
{
public:
    FileDownloadServiceMock(std::string deviceKey, wolkabout::JsonDownloadProtocol& protocol, std::string fileDownloadDirectory,
                            std::uint64_t maxPacketSize, wolkabout::ConnectivityService& connectivityService,
                            wolkabout::FileRepository& fileRepository, std::shared_ptr<wolkabout::UrlFileDownloader> urlFileDownloader)
    : FileDownloadService(deviceKey, protocol, fileDownloadDirectory, maxPacketSize, connectivityService, fileRepository, urlFileDownloader)
    {
    }

    MOCK_METHOD(void, messageReceived, (std::shared_ptr<wolkabout::Message>), (override));
    MOCK_METHOD(void, sendFileList, ());
};

#endif // WOLKABOUTCONNECTOR_FILEDOWNLOADSERVICEMOCK_H

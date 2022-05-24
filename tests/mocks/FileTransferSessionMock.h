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

#ifndef WOLKABOUTCONNECTOR_FILETRANSFERSESSIONMOCK_H
#define WOLKABOUTCONNECTOR_FILETRANSFERSESSIONMOCK_H

#include "core/model/messages/FileBinaryRequestMessage.h"
#include "core/model/messages/FileBinaryResponseMessage.h"
#include "core/model/messages/FileUploadInitiateMessage.h"
#include "wolk/service/file_management/FileTransferSession.h"

#include <gmock/gmock.h>

using namespace wolkabout::connect;

class FileTransferSessionMock : public FileTransferSession
{
public:
    FileTransferSessionMock()
    : FileTransferSession(
        "", FileUploadInitiateMessage{"", 0, ""}, [&](FileTransferStatus, FileTransferError) {}, buffer)
    {
    }

    ~FileTransferSessionMock() override = default;

    MOCK_METHOD(bool, isPlatformTransfer, (), (const));
    MOCK_METHOD(bool, isUrlDownload, (), (const));
    MOCK_METHOD(bool, isDone, (), (const));
    MOCK_METHOD(const std::string&, getDeviceKey, (), (const));
    MOCK_METHOD(const std::string&, getName, (), (const));
    MOCK_METHOD(const std::string&, getUrl, (), (const));
    MOCK_METHOD(void, abort, ());
    MOCK_METHOD(FileTransferError, pushChunk, (const FileBinaryResponseMessage&));
    MOCK_METHOD(FileBinaryRequestMessage, getNextChunkRequest, ());
    MOCK_METHOD(bool, triggerDownload, ());
    MOCK_METHOD(FileTransferStatus, getStatus, (), (const));
    MOCK_METHOD(FileTransferError, getError, (), (const));
    MOCK_METHOD(const std::vector<FileChunk>&, getChunks, (), (const));

private:
    CommandBuffer buffer;
};

#endif    // WOLKABOUTCONNECTOR_FILETRANSFERSESSIONMOCK_H

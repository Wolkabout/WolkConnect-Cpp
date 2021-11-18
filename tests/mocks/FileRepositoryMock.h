/**
 * Copyright 2021 WolkAbout Technology s.r.o.
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
#ifndef WOLKABOUTCONNECTOR_FILEREPOSITORYMOCK_H
#define WOLKABOUTCONNECTOR_FILEREPOSITORYMOCK_H

#include "repository/FileRepository.h"

#include <gmock/gmock.h>

class FileRepositoryMock : public wolkabout::FileRepository
{
public:
    MOCK_METHOD(void, connected, ());
    MOCK_METHOD(void, disconnected, ());

    MOCK_METHOD(std::unique_ptr<wolkabout::FileInfo>, getFileInfo, (const std::string&));
    MOCK_METHOD(std::unique_ptr<std::vector<std::string>>, getAllFileNames, ());

    MOCK_METHOD(void, store, (const wolkabout::FileInfo&));

    MOCK_METHOD(void, remove, (const std::string&));
    MOCK_METHOD(void, removeAll, ());

    MOCK_METHOD(bool, containsInfoForFile, (const std::string&));
};

#endif    // WOLKABOUTCONNECTOR_FILEREPOSITORYMOCK_H

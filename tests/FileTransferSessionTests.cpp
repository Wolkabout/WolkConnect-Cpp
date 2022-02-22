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

#include <any>
#include <sstream>

#define private public
#define protected public
#include "wolk/service/file_management/FileTransferSession.h"
#undef private
#undef protected

#include "core/model/messages/FileBinaryRequestMessage.h"
#include "core/model/messages/FileBinaryResponseMessage.h"
#include "core/model/messages/FileUploadInitiateMessage.h"
#include "core/utilities/Logger.h"

#include <gtest/gtest.h>

using namespace wolkabout;

class FileTransferSessionTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    const std::string TAG = "FileTransferSessionTests";

    const std::string FILE_NAME = "test.file";

    CommandBuffer commandBuffer;
};

TEST_F(FileTransferSessionTests, CreateSingleTransferChunk)
{
    // Create an initiate message for a transfer where there will be a single chunk
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the callback
    auto callback = [&](FileUploadStatus /** status **/, FileUploadError /** error **/) {};

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{initiate, callback, commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Check some getters
    EXPECT_TRUE(session->isPlatformTransfer());
    EXPECT_FALSE(session->isDone());
    EXPECT_EQ(session->getName(), FILE_NAME);

    // Get the first chunk request message
    auto request = FileBinaryRequestMessage{"", 0};
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    ASSERT_EQ(request.getName(), FILE_NAME);
    EXPECT_EQ(session->getStatus(), FileUploadStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileUploadError::NONE);

    // Return the bytes right away
    auto payload = ByteArray(32, 0);
    for (const auto& byte : bytes)
        payload.emplace_back(byte);
    for (const auto& byte : ByteUtils::hashSHA256(bytes))
        payload.emplace_back(byte);
    auto response = FileBinaryResponseMessage{ByteUtils::toString(payload)};
    ASSERT_EQ(session->pushChunk(response), FileUploadError::NONE);

    // Check the values
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileUploadStatus::FILE_READY);
    EXPECT_EQ(session->getError(), FileUploadError::NONE);

    // Expect that the next request returned is not valid
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    ASSERT_TRUE(request.getName().empty());
}

TEST_F(FileTransferSessionTests, AbortFileTransfer)
{
    // Create an initiate message for a transfer where there will be a single chunk
    auto bytes = ByteArray(1, 1);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the callback
    auto callback = [&](FileUploadStatus /** status **/, FileUploadError /** error **/) {};

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{initiate, callback, commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Request the first chunk
    auto request = FileBinaryRequestMessage{"", 0};
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    ASSERT_EQ(request.getName(), FILE_NAME);

    // Now abort the session
    ASSERT_NO_FATAL_FAILURE(session->abort());
    EXPECT_EQ(session->getStatus(), FileUploadStatus::ABORTED);
    EXPECT_EQ(session->getError(), FileUploadError::NONE);

    // Expect that you can't push a chunk now, nor get the next request
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    EXPECT_TRUE(request.getName().empty());
    auto payload = ByteArray(32, 0);
    for (const auto& byte : bytes)
        payload.emplace_back(byte);
    for (const auto& byte : ByteUtils::hashSHA256(bytes))
        payload.emplace_back(byte);
    auto response = FileBinaryResponseMessage(ByteUtils::toString(payload));
    ASSERT_TRUE(session->getChunks().empty());
    ASSERT_EQ(session->pushChunk(response), FileUploadError::NONE);
    ASSERT_TRUE(session->getChunks().empty());
}

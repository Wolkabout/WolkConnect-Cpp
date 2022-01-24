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
#include "core/model/messages/FileUrlDownloadInitMessage.h"
#include "core/utilities/Logger.h"
#include "core/utilities/Timer.h"
#include "mocks/FileDownloaderMock.h"

#include <gtest/gtest.h>

using namespace wolkabout;
using namespace wolkabout::connect;

class FileTransferSessionTests : public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE);
        fileDownloaderMock = std::make_shared<FileDownloaderMock>();
    }

    static std::shared_ptr<FileDownloaderMock> fileDownloaderMock;

    const std::string DEVICE_KEY = "DEVICE_KEY";

    const std::string FILE_NAME = "test.file";

    CommandBuffer commandBuffer;

    std::mutex mutex;

    std::condition_variable conditionVariable;

    Timer timer;
};

std::shared_ptr<FileDownloaderMock> FileTransferSessionTests::fileDownloaderMock;

TEST_F(FileTransferSessionTests, TransferSessionTriggerDownload)
{
    // Create an initiate message for a transfer where there will be a single chunk
    auto bytes = ByteArray(1, 1);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{
      DEVICE_KEY, initiate, [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {},
      commandBuffer}));
    ASSERT_NE(session, nullptr);
    EXPECT_FALSE(session->triggerDownload());
}

TEST_F(FileTransferSessionTests, CreateSingleTransferChunk)
{
    // Create an initiate message for a transfer where there will be a single chunk
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the callback
    auto callback = [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {};

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{DEVICE_KEY, initiate, callback, commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Check some getters
    EXPECT_TRUE(session->isPlatformTransfer());
    EXPECT_FALSE(session->isDone());
    EXPECT_EQ(session->getName(), FILE_NAME);

    // Get the first chunk request message
    auto request = FileBinaryRequestMessage{"", 0};
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    ASSERT_EQ(request.getName(), FILE_NAME);
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Return the bytes right away
    auto payload = ByteArray(32, 0);
    for (const auto& byte : bytes)
        payload.emplace_back(byte);
    for (const auto& byte : ByteUtils::hashSHA256(bytes))
        payload.emplace_back(byte);
    auto response = FileBinaryResponseMessage{ByteUtils::toString(payload)};
    ASSERT_EQ(session->pushChunk(response), FileTransferError::NONE);

    // Check the values
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_READY);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Expect that the next request returned is not valid
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    ASSERT_TRUE(request.getName().empty());
}

TEST_F(FileTransferSessionTests, MultiChunkSession)
{
    // Create the message
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the two chunks
    auto firstMessage = [&] {
        auto currentBytes = ByteArray(32, 0);
        const auto firstBytes = ByteArray(64, 65);
        for (const auto& byte : firstBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(firstBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();
    auto secondMessage = [&] {
        auto currentBytes = std::vector<std::uint8_t>{firstMessage.cend() - 32, firstMessage.cend()};
        const auto secondBytes = ByteArray(36, 65);
        for (const auto& byte : secondBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(secondBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{
      DEVICE_KEY, initiate, [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {},
      commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Get the first chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
              FileTransferError::NONE);

    // Check the values
    ASSERT_FALSE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Get the second chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(secondMessage)}),
              FileTransferError::NONE);

    // Check the values
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_READY);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);
}

TEST_F(FileTransferSessionTests, TransferMoreThanNecessaryBytes)
{
    // Create the message
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the two chunks
    auto firstMessage = [&] {
        auto currentBytes = ByteArray(32, 0);
        const auto firstBytes = ByteArray(100, 65);
        for (const auto& byte : firstBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(firstBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{
      DEVICE_KEY, initiate, [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {},
      commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Get the first chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
              FileTransferError::NONE);

    // Check the values
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_READY);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Malfunctions hahaha
    session->m_done = false;
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
              FileTransferError::UNSUPPORTED_FILE_SIZE);
    ASSERT_TRUE(session->getNextChunkRequest().getName().empty());
}

TEST_F(FileTransferSessionTests, PushCurrentChunkHashToLimits)
{
    // Create the message
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the two chunks
    auto firstMessage = [&] {
        auto currentBytes = ByteArray(32, 0);
        const auto firstBytes = ByteArray(100, 65);
        for (const auto& byte : firstBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(firstBytes))
            currentBytes.emplace_back(byte + 32);
        return currentBytes;
    }();

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{
      DEVICE_KEY, initiate, [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {},
      commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Get the first chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    for (auto i = 0; i < 3; ++i)
        ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
                  FileTransferError::FILE_HASH_MISMATCH);

    // Check the values
    ASSERT_FALSE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Push it to limits
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
              FileTransferError::RETRY_COUNT_EXCEEDED);

    // Check the values
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::ERROR);
    EXPECT_EQ(session->getError(), FileTransferError::RETRY_COUNT_EXCEEDED);
}

TEST_F(FileTransferSessionTests, SecondChunkRepeatedlyGetsThePreviousHashWrong)
{
    // Create the message
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the two chunks
    auto firstMessage = [&] {
        auto currentBytes = ByteArray(32, 0);
        const auto firstBytes = ByteArray(64, 65);
        for (const auto& byte : firstBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(firstBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();
    auto secondMessage = [&] {
        auto currentBytes = std::vector<std::uint8_t>{firstMessage.cend() - 64, firstMessage.cend() - 32};
        const auto secondBytes = ByteArray(36, 65);
        for (const auto& byte : secondBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(secondBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{
      DEVICE_KEY, initiate, [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {},
      commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Get the first chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
              FileTransferError::NONE);

    // Check the values
    ASSERT_FALSE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Get the second chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    for (auto i = 0; i < 3; ++i)
        ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(secondMessage)}),
                  FileTransferError::FILE_HASH_MISMATCH);
    ASSERT_FALSE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Check the values
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(secondMessage)}),
              FileTransferError::RETRY_COUNT_EXCEEDED);
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::ERROR);
    EXPECT_EQ(session->getError(), FileTransferError::RETRY_COUNT_EXCEEDED);
}

TEST_F(FileTransferSessionTests, MultiChunkFinalHashIsNotRight)
{
    // Create the message
    auto bytes = ByteArray(100, 65);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the two chunks
    auto firstMessage = [&] {
        auto currentBytes = ByteArray(32, 0);
        const auto firstBytes = ByteArray(64, 65);
        for (const auto& byte : firstBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(firstBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();
    auto secondMessage = [&] {
        auto currentBytes = std::vector<std::uint8_t>{firstMessage.cend() - 32, firstMessage.cend()};
        const auto secondBytes = ByteArray(36, 69);
        for (const auto& byte : secondBytes)
            currentBytes.emplace_back(byte);
        for (const auto& byte : ByteUtils::hashSHA256(secondBytes))
            currentBytes.emplace_back(byte);
        return currentBytes;
    }();

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{
      DEVICE_KEY, initiate, [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {},
      commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Get the first chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(firstMessage)}),
              FileTransferError::NONE);

    // Check the values
    ASSERT_FALSE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_TRANSFER);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

    // Get the second chunk request message
    ASSERT_NO_FATAL_FAILURE(session->getNextChunkRequest());
    ASSERT_EQ(session->pushChunk(FileBinaryResponseMessage{ByteUtils::toString(secondMessage)}),
              FileTransferError::NONE);

    // Check the values
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::ERROR);
    EXPECT_EQ(session->getError(), FileTransferError::FILE_HASH_MISMATCH);
}

TEST_F(FileTransferSessionTests, AbortFileTransfer)
{
    // Create an initiate message for a transfer where there will be a single chunk
    auto bytes = ByteArray(1, 1);
    auto hash = ByteUtils::hashMDA5(bytes);
    auto initiate = FileUploadInitiateMessage{FILE_NAME, bytes.size(), ByteUtils::toHexString(hash)};

    // Create the callback
    auto callback = [&](FileTransferStatus /** status **/, FileTransferError /** error **/) {};

    // Make place for the session
    auto session = std::unique_ptr<FileTransferSession>{};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{DEVICE_KEY, initiate, callback, commandBuffer}));
    ASSERT_NE(session, nullptr);

    // Request the first chunk
    auto request = FileBinaryRequestMessage{"", 0};
    ASSERT_NO_FATAL_FAILURE(request = session->getNextChunkRequest());
    ASSERT_EQ(request.getName(), FILE_NAME);

    // Now abort the session
    ASSERT_NO_FATAL_FAILURE(session->abort());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::ABORTED);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);

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
    ASSERT_EQ(session->pushChunk(response), FileTransferError::NONE);
    ASSERT_TRUE(session->getChunks().empty());
}

TEST_F(FileTransferSessionTests, InvalidUrlSessionThings)
{
    // React to the response from the session
    auto callback = [&](FileTransferStatus, FileTransferError) {};

    // Make the session
    auto url = "https://test.url/" + FILE_NAME;
    auto session = std::unique_ptr<FileTransferSession>{};
    auto init = FileUrlDownloadInitMessage{url};
    ASSERT_NO_FATAL_FAILURE(session.reset(new FileTransferSession{DEVICE_KEY, init, callback, commandBuffer, nullptr}));

    // Expect that you can't start this
    EXPECT_FALSE(session->triggerDownload());
    EXPECT_EQ(session->pushChunk(FileBinaryResponseMessage{""}), FileTransferError::TRANSFER_PROTOCOL_DISABLED);
    EXPECT_TRUE(session->getNextChunkRequest().getName().empty());
}

TEST_F(FileTransferSessionTests, SimpleUrlDownloadSession)
{
    // React to the response from the session
    auto callback = [&](FileTransferStatus status, FileTransferError) {
        if (status == wolkabout::FileTransferStatus::FILE_READY)
            conditionVariable.notify_one();
    };

    // Make the session
    auto url = "https://test.url/" + FILE_NAME;
    auto session = std::unique_ptr<FileTransferSession>{};
    auto init = FileUrlDownloadInitMessage{url};
    ASSERT_NO_FATAL_FAILURE(
      session.reset(new FileTransferSession{DEVICE_KEY, init, callback, commandBuffer, fileDownloaderMock}));

    // Check some getters
    EXPECT_TRUE(session->isUrlDownload());
    EXPECT_FALSE(session->isDone());
    EXPECT_EQ(session->getDeviceKey(), DEVICE_KEY);
    EXPECT_EQ(session->getUrl(), url);
    EXPECT_TRUE(session->getName().empty());

    // Expect the call on the downloader
    const auto delay = std::chrono::milliseconds{50};
    EXPECT_CALL(*fileDownloaderMock, downloadFile)
      .WillOnce([&](std::string downloadUrl,
                    std::function<void(FileTransferStatus, FileTransferError, const std::string&)> statusCallback) {
          if (downloadUrl == url)
              timer.start(delay, [this, statusCallback] {
                  statusCallback(wolkabout::FileTransferStatus::FILE_READY, wolkabout::FileTransferError::NONE,
                                 FILE_NAME);
              });
      });

    // Now wait for the condition variable to be invoked
    auto timeout = std::chrono::milliseconds{100};

    // And measure the time
    const auto start = std::chrono::system_clock::now();
    std::unique_lock<std::mutex> lock{mutex};
    ASSERT_NO_FATAL_FAILURE(session->triggerDownload());
    conditionVariable.wait_for(lock, timeout);
    const auto duration = std::chrono::system_clock::now() - start;
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    const auto tolerable = delay * 1.5;
    LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
              << "ms) - Delay time: " << tolerable.count() << "ms.";
    ASSERT_LT(durationMs.count(), tolerable.count());

    // And check the session
    ASSERT_TRUE(session->isDone());
    EXPECT_EQ(session->getStatus(), FileTransferStatus::FILE_READY);
    EXPECT_EQ(session->getError(), FileTransferError::NONE);
    EXPECT_EQ(session->getName(), FILE_NAME);
}

TEST_F(FileTransferSessionTests, AbortUrlTransfer)
{
    // React to the response from the session
    auto callback = [&](FileTransferStatus status, FileTransferError) {
        if (status == FileTransferStatus::ABORTED)
            conditionVariable.notify_one();
    };

    // Make the session
    auto url = "https://test.url/" + FILE_NAME;
    auto session = std::unique_ptr<FileTransferSession>{};
    auto init = FileUrlDownloadInitMessage{url};
    ASSERT_NO_FATAL_FAILURE(
      session.reset(new FileTransferSession{DEVICE_KEY, init, callback, commandBuffer, fileDownloaderMock}));

    // Expect the call
    EXPECT_CALL(*fileDownloaderMock, abortDownload);
    ASSERT_NO_FATAL_FAILURE(session->abort());

    auto timeout = std::chrono::milliseconds{100};

    if (session->getStatus() != FileTransferStatus::ABORTED)
    {
        const auto start = std::chrono::system_clock::now();
        auto lock = std::unique_lock<std::mutex>{mutex};
        conditionVariable.wait_for(lock, timeout);
        const auto duration = std::chrono::system_clock::now() - start;
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        EXPECT_LT(durationMs, timeout);
        LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
                  << "ms) - Timeout time: " << timeout.count() << "ms.";
    }
    EXPECT_EQ(session->getStatus(), FileTransferStatus::ABORTED);
}

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
#include "wolk/service/file_management/FileManagementService.h"
#undef private
#undef protected

#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"
#include "mocks/ConnectivityServiceMock.h"
#include "mocks/DataProtocolMock.h"
#include "mocks/DataServiceMock.h"
#include "mocks/FileDownloaderMock.h"
#include "mocks/FileListenerMock.h"
#include "mocks/FileManagementProtocolMock.h"
#include "mocks/FileTransferSessionMock.h"
#include "mocks/PersistenceMock.h"

#include <gtest/gtest.h>

using namespace wolkabout::connect;
using namespace ::testing;

class FileManagementServiceTests : public ::testing::Test
{
public:
    void SetUp() override
    {
        _internalFeedUpdateSetHandler = [&](std::string deviceKey,
                                            std::map<std::uint64_t, std::vector<Reading>> readings) {
            if (feedUpdateSetHandler)
                feedUpdateSetHandler(std::move(deviceKey), std::move(readings));
        };
        _internalParameterSyncHandler = [&](std::string deviceKey, std::vector<Parameter> parameters) {
            if (parameterSyncHandler)
                parameterSyncHandler(std::move(deviceKey), std::move(parameters));
        };

        connectivityServiceMock = std::make_shared<NiceMock<ConnectivityServiceMock>>();
        dataServiceMock =
          std::make_shared<NiceMock<DataServiceMock>>(dataProtocolMock, *persistenceMock, *connectivityServiceMock,
                                                      _internalFeedUpdateSetHandler, _internalParameterSyncHandler);
        fileDownloaderMock = std::make_shared<NiceMock<FileDownloaderMock>>();
        fileListenerMock = std::make_shared<NiceMock<FileListenerMock>>();
        service = std::unique_ptr<FileManagementService>{
          new FileManagementService{*connectivityServiceMock, *dataServiceMock, fileManagementProtocolMock,
                                    fileLocation, true, true, fileDownloaderMock, fileListenerMock}};
        service->createFolder();
    }

    void TearDown() override { DeleteEverything(); }

    void DeleteEverything()
    {
        const auto subFolderPath = FileSystemUtils::composePath(DEVICE_KEY, fileLocation);
        if (FileSystemUtils::isDirectoryPresent(subFolderPath))
        {
            const auto files = FileSystemUtils::listFiles(subFolderPath);
            for (const auto& file : files)
            {
                const auto filePath = FileSystemUtils::composePath(file, subFolderPath);
                if (!FileSystemUtils::deleteFile(filePath))
                    LOG(ERROR) << "Failed to delete '" << filePath << "'.";
            }
            if (!FileSystemUtils::deleteFile(subFolderPath))
                LOG(ERROR) << "Failed to delete '" << subFolderPath << "'.";
        }
        if (!FileSystemUtils::deleteFile(fileLocation))
            LOG(ERROR) << "Failed to delete '" << fileLocation << "'.";
    }

    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    static std::string fileLocation;

    std::shared_ptr<ConnectivityServiceMock> connectivityServiceMock;

    DataProtocolMock dataProtocolMock;

    std::shared_ptr<PersistenceMock> persistenceMock;

    std::shared_ptr<DataServiceMock> dataServiceMock;

    FileManagementProtocolMock fileManagementProtocolMock;

    std::shared_ptr<FileDownloaderMock> fileDownloaderMock;

    std::shared_ptr<FileListenerMock> fileListenerMock;

    std::unique_ptr<FileManagementService> service;

    FeedUpdateSetHandler feedUpdateSetHandler;

    ParameterSyncHandler parameterSyncHandler;

    std::mutex mutex;
    std::condition_variable conditionVariable;

    const std::string DEVICE_KEY = "TestDevice";

    const std::string TEST_FILE = "test.file";
    const std::uint64_t TEST_FILE_SIZE = 256;
    const std::string TEST_FILE_HASH = "test.hash";

    const std::string TEST_PATH = "http://test.location/test.file";

private:
    FeedUpdateSetHandler _internalFeedUpdateSetHandler;
    ParameterSyncHandler _internalParameterSyncHandler;
};

std::string FileManagementServiceTests::fileLocation = "./test-fm-folder";

TEST_F(FileManagementServiceTests, NeitherEnabled)
{
    ASSERT_THROW(FileManagementService(*connectivityServiceMock, *dataServiceMock, fileManagementProtocolMock,
                                       fileLocation, false, false, fileDownloaderMock, fileListenerMock),
                 std::runtime_error);
}

TEST_F(FileManagementServiceTests, ProtocolCheck)
{
    EXPECT_EQ(&(service->getProtocol()), &fileManagementProtocolMock);
    EXPECT_TRUE(service->isFileTransferEnabled());
    EXPECT_TRUE(service->isFileTransferUrlEnabled());
}

TEST_F(FileManagementServiceTests, CreateTheFolder)
{
    // Check for the folder
    if (FileSystemUtils::isDirectoryPresent(fileLocation))
        FileSystemUtils::deleteFile(fileLocation);

    // Create the folder
    ASSERT_NO_FATAL_FAILURE(service->createFolder());
    EXPECT_TRUE(FileSystemUtils::isDirectoryPresent(fileLocation));
}

TEST_F(FileManagementServiceTests, ReceiveMessageInvalidMessages)
{
    // One where the message is null
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(nullptr));

    // One where the message type is not handled by this service
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::UNKNOWN));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(""));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, NotifyAddedFileTest)
{
    // Make the listener invoke the condition variable
    std::atomic_bool called{false};
    EXPECT_CALL(*fileListenerMock, onAddedFile)
      .WillOnce([&](const std::string&, const std::string&, const std::string&) {
          called = true;
          conditionVariable.notify_one();
      });

    // Call the service and measure the execution time
    const auto timeout = std::chrono::milliseconds{100};
    auto lock = std::unique_lock<std::mutex>{mutex};
    const auto start = std::chrono::system_clock::now();
    ASSERT_NO_FATAL_FAILURE(service->notifyListenerAddedFile(DEVICE_KEY, TEST_FILE, ""));
    if (!called)
    {
        conditionVariable.wait_for(lock, timeout);
        const auto duration = std::chrono::system_clock::now() - start;
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
                  << "ms) - Timeout time: " << timeout.count() << "ms.";
        ASSERT_LT(durationMs.count(), timeout.count());
    }
    EXPECT_TRUE(called);
}

TEST_F(FileManagementServiceTests, NotifyRemovedFileTest)
{
    // Make the listener invoke the condition variable
    std::atomic_bool called{false};
    EXPECT_CALL(*fileListenerMock, onRemovedFile).WillOnce([&](const std::string&, const std::string&) {
        called = true;
        conditionVariable.notify_one();
    });

    // Call the service and measure the execution time
    const auto timeout = std::chrono::milliseconds{100};
    auto lock = std::unique_lock<std::mutex>{mutex};
    const auto start = std::chrono::system_clock::now();
    ASSERT_NO_FATAL_FAILURE(service->notifyListenerRemovedFile(DEVICE_KEY, TEST_FILE));
    if (!called)
    {
        conditionVariable.wait_for(lock, timeout);
        const auto duration = std::chrono::system_clock::now() - start;
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        LOG(INFO) << "Execution time: " << duration.count() << "μs (" << durationMs.count()
                  << "ms) - Timeout time: " << timeout.count() << "ms.";
        ASSERT_LT(durationMs.count(), timeout.count());
    }
    EXPECT_TRUE(called);
}

TEST_F(FileManagementServiceTests, GetAbsolutePathOfFile)
{
    ASSERT_NO_FATAL_FAILURE(service->createFolder());
    ASSERT_TRUE(FileSystemUtils::createDirectory(FileSystemUtils::composePath(DEVICE_KEY, fileLocation)));
    const auto fullFilePath =
      FileSystemUtils::composePath(TEST_FILE, FileSystemUtils::composePath(DEVICE_KEY, fileLocation));
    ASSERT_TRUE(FileSystemUtils::createFileWithContent(fullFilePath, "Hello!"));

    auto path = std::string{};
    ASSERT_NO_FATAL_FAILURE(path = service->absolutePathOfFile(DEVICE_KEY, TEST_FILE));
    LOG(TRACE) << "Absolute path: '" << path << "'.";
    EXPECT_GT(path.size(), DEVICE_KEY.size() + TEST_FILE.size());
    EXPECT_NE(path.find(DEVICE_KEY + "/" + TEST_FILE), std::string::npos);
}

TEST_F(FileManagementServiceTests, ReportTransferDisabledFailedToGenerateMessage)
{
    // Make the protocol fail to return
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->reportTransferProtocolDisabled(DEVICE_KEY, TEST_FILE));
}

TEST_F(FileManagementServiceTests, ReportTransferDisabledValid)
{
    // Make the protocol fail to return
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce([&](const std::string&, const FileUploadStatusMessage& message) {
          EXPECT_EQ(message.getStatus(), FileTransferStatus::ERROR);
          EXPECT_EQ(message.getError(), FileTransferError::TRANSFER_PROTOCOL_DISABLED);
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    ASSERT_NO_FATAL_FAILURE(service->reportTransferProtocolDisabled(DEVICE_KEY, TEST_FILE));
}

TEST_F(FileManagementServiceTests, ReportUrlDownloadDisabledFailedToGenerateMessage)
{
    // Make the protocol fail to return
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUrlDownloadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->reportUrlTransferProtocolDisabled(DEVICE_KEY, TEST_FILE));
}

TEST_F(FileManagementServiceTests, ReportUrlDownloadDisabledValid)
{
    // Make the protocol fail to return
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUrlDownloadStatusMessage&>()))
      .WillOnce([&](const std::string&, const FileUrlDownloadStatusMessage& message) {
          EXPECT_EQ(message.getStatus(), FileTransferStatus::ERROR);
          EXPECT_EQ(message.getError(), FileTransferError::TRANSFER_PROTOCOL_DISABLED);
          return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
      });
    ASSERT_NO_FATAL_FAILURE(service->reportUrlTransferProtocolDisabled(DEVICE_KEY, TEST_FILE));
}

TEST_F(FileManagementServiceTests, ObtainFileInformationForFileThatDoesntExist)
{
    // Make place for the returned value
    auto information = FileInformation{};
    ASSERT_NO_FATAL_FAILURE(information = service->obtainFileInformation(DEVICE_KEY, TEST_FILE));
    EXPECT_TRUE(information.name.empty());
    EXPECT_EQ(information.size, 0);
    EXPECT_TRUE(information.hash.empty());
}

TEST_F(FileManagementServiceTests, ObtainFileInformation)
{
    // Define the file contents
    const auto fileContent = ByteArray(100, 65);
    const auto name = "TestFile";
    const auto size = 100;
    const auto hash = ByteUtils::toHexString(ByteUtils::hashSHA256(fileContent));

    // Now place the file in the file system
    const auto subFolder = FileSystemUtils::composePath(DEVICE_KEY, fileLocation);
    ASSERT_TRUE(FileSystemUtils::createDirectory(subFolder));
    ASSERT_TRUE(
      FileSystemUtils::createBinaryFileWithContent(FileSystemUtils::composePath(name, subFolder), fileContent));

    // And now test service
    auto information = FileInformation{};
    ASSERT_NO_FATAL_FAILURE(information = service->obtainFileInformation(DEVICE_KEY, name));
    EXPECT_EQ(information.name, name);
    EXPECT_EQ(information.size, size);
    EXPECT_EQ(information.hash, hash);
}

TEST_F(FileManagementServiceTests, SendChunkRequestNullptr)
{
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileBinaryRequestMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->sendChunkRequest(DEVICE_KEY, FileBinaryRequestMessage{TEST_FILE, 0}));
}

TEST_F(FileManagementServiceTests, SendChunkRequest)
{
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileBinaryRequestMessage&>()))
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(*connectivityServiceMock, publish);
    ASSERT_NO_FATAL_FAILURE(service->sendChunkRequest(DEVICE_KEY, FileBinaryRequestMessage{TEST_FILE, 0}));
}

TEST_F(FileManagementServiceTests, ReportStatusForNonExistingSession)
{
    ASSERT_NO_FATAL_FAILURE(service->reportStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_TRANSFER));
}

TEST_F(FileManagementServiceTests, ReportStatusForTransfer)
{
    // Mock the session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).WillOnce(Return(true));
    EXPECT_CALL(*session, getName).WillOnce(ReturnRef(TEST_FILE));
    service->m_sessions[DEVICE_KEY] = std::move(session);

    // Check that the protocol and connectivity service get called
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce([&](const std::string&, const FileUploadStatusMessage& status) -> std::unique_ptr<wolkabout::Message> {
          if (status.getStatus() == wolkabout::FileTransferStatus::FILE_READY &&
              status.getError() == wolkabout::FileTransferError::UNSUPPORTED_FILE_SIZE)
              return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
          return nullptr;
      });
    EXPECT_CALL(*connectivityServiceMock, publish);

    // And now report the session
    ASSERT_NO_FATAL_FAILURE(service->reportStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_READY,
                                                  wolkabout::FileTransferError::UNSUPPORTED_FILE_SIZE));
}

TEST_F(FileManagementServiceTests, ReportStatusForUrlDownload)
{
    // Mock the session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).WillOnce(Return(false));
    EXPECT_CALL(*session, getUrl).WillOnce(ReturnRef(TEST_FILE));
    EXPECT_CALL(*session, getName).WillOnce(ReturnRef(TEST_FILE));
    service->m_sessions[DEVICE_KEY] = std::move(session);

    // Check that the protocol and connectivity service get called
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUrlDownloadStatusMessage&>()))
      .WillOnce(
        [&](const std::string&, const FileUrlDownloadStatusMessage& status) -> std::unique_ptr<wolkabout::Message> {
            if (status.getStatus() == wolkabout::FileTransferStatus::FILE_READY &&
                status.getError() == wolkabout::FileTransferError::UNSUPPORTED_FILE_SIZE)
                return std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}};
            return nullptr;
        });
    EXPECT_CALL(*connectivityServiceMock, publish);

    // And now report the session
    ASSERT_NO_FATAL_FAILURE(service->reportStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_READY,
                                                  wolkabout::FileTransferError::UNSUPPORTED_FILE_SIZE));
}

TEST_F(FileManagementServiceTests, OnSessionStatus)
{
    ASSERT_NO_FATAL_FAILURE(service->onFileSessionStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_TRANSFER));
}

TEST_F(FileManagementServiceTests, OnSessionStatusAborted)
{
    // Inject a session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).WillOnce(Return(true));
    EXPECT_CALL(*session, getName).WillOnce(ReturnRef(TEST_FILE));
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));

    // Call session status
    ASSERT_NO_FATAL_FAILURE(service->onFileSessionStatus(DEVICE_KEY, wolkabout::FileTransferStatus::ABORTED));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Check that it was deleted
    EXPECT_EQ(service->m_sessions[DEVICE_KEY], nullptr);
}

TEST_F(FileManagementServiceTests, OnSessionStatusError)
{
    // Inject a session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).WillOnce(Return(true));
    EXPECT_CALL(*session, getName).WillOnce(ReturnRef(TEST_FILE));
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));

    // Call session status
    ASSERT_NO_FATAL_FAILURE(service->onFileSessionStatus(DEVICE_KEY, wolkabout::FileTransferStatus::ERROR));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Check that it was deleted
    EXPECT_EQ(service->m_sessions[DEVICE_KEY], nullptr);
}

TEST_F(FileManagementServiceTests, OnSessionStatusReadyPlatformTransfer)
{
    // Inject a session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*session, getName).Times(2).WillRepeatedly(ReturnRef(TEST_FILE));
    EXPECT_CALL(*session, getDeviceKey).WillOnce(ReturnRef(DEVICE_KEY));
    const auto bytes = ByteArray{65, 65, 65, 65, 65};
    const auto chunks =
      std::vector<FileChunk>{FileChunk{"", bytes, ByteUtils::toHexString(ByteUtils::hashSHA256(bytes))}};
    EXPECT_CALL(*session, getChunks).WillOnce(ReturnRef(chunks));
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));

    // Call session status
    ASSERT_NO_FATAL_FAILURE(service->onFileSessionStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_READY));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Check that it was deleted
    EXPECT_EQ(service->m_sessions[DEVICE_KEY], nullptr);
    const auto filePath =
      FileSystemUtils::composePath(TEST_FILE, FileSystemUtils::composePath(DEVICE_KEY, fileLocation));
    EXPECT_TRUE(FileSystemUtils::isFilePresent(filePath));
    auto fileContent = std::string{};
    ASSERT_TRUE(FileSystemUtils::readFileContent(filePath, fileContent));
    EXPECT_EQ(fileContent, "AAAAA");
}

TEST_F(FileManagementServiceTests, OnSessionStatusReadyUrlDownload)
{
    // Inject a session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(*session, getUrl).Times(1).WillRepeatedly(ReturnRef(TEST_FILE));
    EXPECT_CALL(*session, getName).Times(2).WillRepeatedly(ReturnRef(TEST_FILE));
    EXPECT_CALL(*session, getDeviceKey).WillOnce(ReturnRef(DEVICE_KEY));
    const auto bytes = ByteArray{69, 69, 69, 69};
    EXPECT_CALL(*fileDownloaderMock, getBytes).WillOnce(ReturnRef(bytes));
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUrlDownloadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));

    // Call session status
    ASSERT_NO_FATAL_FAILURE(service->onFileSessionStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_READY));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Check that it was deleted
    EXPECT_EQ(service->m_sessions[DEVICE_KEY], nullptr);
    const auto filePath =
      FileSystemUtils::composePath(TEST_FILE, FileSystemUtils::composePath(DEVICE_KEY, fileLocation));
    EXPECT_TRUE(FileSystemUtils::isFilePresent(filePath));
    auto fileContent = std::string{};
    ASSERT_TRUE(FileSystemUtils::readFileContent(filePath, fileContent));
    EXPECT_EQ(fileContent, "EEEE");
}

TEST_F(FileManagementServiceTests, OnSessionStatusReadyInvalidName)
{
    // Inject a session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).Times(3).WillRepeatedly(Return(true));
    const auto fakeName = std::string{"/" + TEST_FILE + "/"};
    EXPECT_CALL(*session, getName).Times(3).WillRepeatedly(ReturnRef(fakeName));
    EXPECT_CALL(*session, getDeviceKey).WillOnce(ReturnRef(DEVICE_KEY));
    const auto bytes = ByteArray{65, 65, 65, 65, 65};
    const auto chunks =
      std::vector<FileChunk>{FileChunk{"", bytes, ByteUtils::toHexString(ByteUtils::hashSHA256(bytes))}};
    EXPECT_CALL(*session, getChunks).WillOnce(ReturnRef(chunks));
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)))
      .WillOnce(Return(ByMove(nullptr)));

    // Call session status
    ASSERT_NO_FATAL_FAILURE(service->onFileSessionStatus(DEVICE_KEY, wolkabout::FileTransferStatus::FILE_READY));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Check that it was deleted
    EXPECT_EQ(service->m_sessions[DEVICE_KEY], nullptr);
    const auto filePath =
      FileSystemUtils::composePath(TEST_FILE, FileSystemUtils::composePath(DEVICE_KEY, fileLocation));
    EXPECT_FALSE(FileSystemUtils::isFilePresent(filePath));
}

TEST_F(FileManagementServiceTests, TransferInitInvalidMessage)
{
    ASSERT_NO_FATAL_FAILURE(service->onFileUploadInit(DEVICE_KEY, FileUploadInitiateMessage{"", 0, ""}));
}

TEST_F(FileManagementServiceTests, TransferInitAlreadyExistingSession)
{
    // Emplace the session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NO_FATAL_FAILURE(
      service->onFileUploadInit(DEVICE_KEY, FileUploadInitiateMessage{TEST_FILE, TEST_FILE_SIZE, TEST_FILE_HASH}));
}

TEST_F(FileManagementServiceTests, TransferInit)
{
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)))
      .WillOnce([&](const std::string&, const FileUploadStatusMessage&) {
          conditionVariable.notify_one();
          return nullptr;
      });
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileBinaryRequestMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(
      service->onFileUploadInit(DEVICE_KEY, FileUploadInitiateMessage{TEST_FILE, TEST_FILE_SIZE, TEST_FILE_HASH}));
    EXPECT_NE(service->m_sessions[DEVICE_KEY], nullptr);

    service->onFileUploadAbort(DEVICE_KEY, FileUploadAbortMessage{TEST_FILE});
    auto lock = std::unique_lock<std::mutex>{mutex};
    conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
}

TEST_F(FileManagementServiceTests, TransferBinaryResponse)
{
    // Inject a session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    EXPECT_CALL(*session, isPlatformTransfer).Times(4).WillRepeatedly(Return(true));
    EXPECT_CALL(*session, pushChunk)
      .WillOnce(Return(FileTransferError::FILE_HASH_MISMATCH))
      .WillOnce(Return(FileTransferError::FILE_HASH_MISMATCH))
      .WillOnce(Return(FileTransferError::FILE_HASH_MISMATCH))
      .WillOnce(Return(FileTransferError::NONE));
    EXPECT_CALL(*session, isDone).WillOnce(Return(true));
    EXPECT_CALL(*session, getNextChunkRequest).Times(3).WillRepeatedly([&]() {
        return FileBinaryRequestMessage{TEST_FILE, 0};
    });
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);

    // Set up the mock
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileBinaryRequestMessage&>()))
      .Times(3)
      .WillRepeatedly([&](const std::string&, const FileBinaryRequestMessage&) { return nullptr; });
    for (auto i = 0; i < 4; ++i)
        ASSERT_NO_FATAL_FAILURE(service->onFileBinaryResponse(DEVICE_KEY, FileBinaryResponseMessage{""}));
}

TEST_F(FileManagementServiceTests, UrlDownloadInitAlreadyExistingSession)
{
    // Emplace the session
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    service->m_sessions[DEVICE_KEY] = std::move(session);
    ASSERT_NO_FATAL_FAILURE(service->onFileUrlDownloadInit(DEVICE_KEY, FileUrlDownloadInitMessage{TEST_PATH}));
}

TEST_F(FileManagementServiceTests, UrlDownloadInit)
{
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUrlDownloadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)))
      .WillOnce([&](const std::string&, const FileUrlDownloadStatusMessage&) {
          conditionVariable.notify_one();
          return nullptr;
      });
    ASSERT_NO_FATAL_FAILURE(service->onFileUrlDownloadInit(DEVICE_KEY, FileUrlDownloadInitMessage{TEST_PATH}));
    ASSERT_NE(service->m_sessions[DEVICE_KEY], nullptr);
    service->onFileUrlDownloadAbort(DEVICE_KEY, FileUrlDownloadAbortMessage{TEST_PATH});
    auto lock = std::unique_lock<std::mutex>{mutex};
    conditionVariable.wait_for(lock, std::chrono::milliseconds{100});
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileTransferInitFailedToParse)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_UPLOAD_INIT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUploadInit).WillOnce(Return(ByMove(nullptr)));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileTransferInitEnabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_UPLOAD_INIT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUploadInit)
      .WillOnce(Return(ByMove(std::unique_ptr<FileUploadInitiateMessage>{
        new FileUploadInitiateMessage{TEST_FILE, TEST_FILE_SIZE, TEST_FILE_HASH}})));
    // Set up the internal calls - we're going to already emplace a session in
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    service->m_sessions[DEVICE_KEY] = std::move(session);
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileTransferInitDisabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_UPLOAD_INIT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUploadInit)
      .WillOnce(Return(ByMove(std::unique_ptr<FileUploadInitiateMessage>{
        new FileUploadInitiateMessage{TEST_FILE, TEST_FILE_SIZE, TEST_FILE_HASH}})));
    // Set up the internal calls - not enabled.
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    service->m_fileTransferEnabled = false;
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileTransferAbortFailedToParse)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_UPLOAD_ABORT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUploadAbort).WillOnce(Return(ByMove(nullptr)));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileTransferAbortEnabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_UPLOAD_ABORT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUploadAbort)
      .WillOnce(Return(ByMove(std::unique_ptr<FileUploadAbortMessage>{new FileUploadAbortMessage{TEST_FILE}})));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileTransferAbortDisabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_UPLOAD_ABORT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUploadAbort)
      .WillOnce(Return(ByMove(std::unique_ptr<FileUploadAbortMessage>{new FileUploadAbortMessage{TEST_FILE}})));
    // Set up the internal calls - not enabled.
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUploadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    service->m_fileTransferEnabled = false;
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileBinaryResponseFailedToParse)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_BINARY_RESPONSE));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileBinaryResponse).WillOnce(Return(ByMove(nullptr)));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileBinaryResponseEnabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_BINARY_RESPONSE));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileBinaryResponse)
      .WillOnce(Return(ByMove(std::unique_ptr<FileBinaryResponseMessage>{new FileBinaryResponseMessage{""}})));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileUrlInitFailedToParse)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_URL_DOWNLOAD_INIT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUrlDownloadInit).WillOnce(Return(ByMove(nullptr)));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileUrlInitEnabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_URL_DOWNLOAD_INIT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUrlDownloadInit)
      .WillOnce(Return(ByMove(std::unique_ptr<FileUrlDownloadInitMessage>{new FileUrlDownloadInitMessage{TEST_PATH}})));
    // Set up the internal calls - we're going to already emplace a session in
    auto session = std::unique_ptr<FileTransferSessionMock>{new FileTransferSessionMock};
    service->m_sessions[DEVICE_KEY] = std::move(session);
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileUrlInitDisabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_URL_DOWNLOAD_INIT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUrlDownloadInit)
      .WillOnce(Return(ByMove(std::unique_ptr<FileUrlDownloadInitMessage>{new FileUrlDownloadInitMessage{TEST_PATH}})));
    // Set up the internal calls - not enabled.
    EXPECT_CALL(fileManagementProtocolMock,
                makeOutboundMessage(A<const std::string&>(), A<const FileUrlDownloadStatusMessage&>()))
      .WillOnce(Return(ByMove(nullptr)));
    service->m_fileTransferUrlEnabled = false;
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileUrlAbortFailedToParse)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_URL_DOWNLOAD_ABORT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUrlDownloadAbort).WillOnce(Return(ByMove(nullptr)));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileUrlAbortEnabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_URL_DOWNLOAD_ABORT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUrlDownloadAbort)
      .WillOnce(
        Return(ByMove(std::unique_ptr<FileUrlDownloadAbortMessage>{new FileUrlDownloadAbortMessage{TEST_FILE}})));
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FileManagementServiceTests, ReceiveMessageFileUrlAbortDisabled)
{
    // Set up the message mock calls
    EXPECT_CALL(fileManagementProtocolMock, getMessageType).WillOnce(Return(MessageType::FILE_URL_DOWNLOAD_ABORT));
    EXPECT_CALL(fileManagementProtocolMock, getDeviceKey).WillOnce(Return(DEVICE_KEY));
    EXPECT_CALL(fileManagementProtocolMock, parseFileUrlDownloadAbort)
      .WillOnce(
        Return(ByMove(std::unique_ptr<FileUrlDownloadAbortMessage>{new FileUrlDownloadAbortMessage{TEST_FILE}})));
    // Set up the internal calls - not enabled.
    service->m_fileTransferEnabled = false;
    // Call the method
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

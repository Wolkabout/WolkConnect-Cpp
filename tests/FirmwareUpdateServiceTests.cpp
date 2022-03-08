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
#include "wolk/service/firmware_update/FirmwareUpdateService.h"
#undef private
#undef protected

#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"
#include "tests/mocks/ConnectivityServiceMock.h"
#include "tests/mocks/DataProtocolMock.h"
#include "tests/mocks/DataServiceMock.h"
#include "tests/mocks/FileManagementProtocolMock.h"
#include "tests/mocks/FileManagementServiceMock.h"
#include "tests/mocks/FirmwareInstallerMock.h"
#include "tests/mocks/FirmwareParametersListenerMock.h"
#include "tests/mocks/FirmwareUpdateProtocolMock.h"
#include "tests/mocks/OutboundMessageHandlerMock.h"
#include "tests/mocks/OutboundRetryMessageHandlerMock.h"
#include "tests/mocks/PersistenceMock.h"

#include <gtest/gtest.h>

#include <vector>

using namespace wolkabout;
using namespace wolkabout::connect;
using namespace ::testing;

class FirmwareUpdateServiceTests : public ::testing::Test
{
public:
    void SetUp() override
    {
        fileManagementServiceMock = std::make_shared<FileManagementServiceMock>(
          connectivityServiceMock, dataServiceMock, fileManagementProtocolMock, "");
        firmwareInstallerMock = std::unique_ptr<FirmwareInstallerMock>{new NiceMock<FirmwareInstallerMock>};
        firmwareParametersListenerMock =
          std::unique_ptr<FirmwareParametersListenerMock>{new NiceMock<FirmwareParametersListenerMock>};
        service = nullptr;
    }

    void TearDown() override
    {
        if (service)
            service->deleteSessionFile(DEVICE_KEY);
    }

    void CreateServiceWithInstaller(const std::string& destination = "./")
    {
        service = std::unique_ptr<FirmwareUpdateService>{
          new FirmwareUpdateService{connectivityServiceMock, dataServiceMock, fileManagementServiceMock,
                                    std::move(firmwareInstallerMock), firmwareUpdateProtocolMock, destination}};
    }

    void CreateServiceWithParameterListener(const std::string& destination = "./")
    {
        service = std::unique_ptr<FirmwareUpdateService>{new FirmwareUpdateService{
          connectivityServiceMock, dataServiceMock, fileManagementServiceMock,
          std::move(firmwareParametersListenerMock), firmwareUpdateProtocolMock, destination}};
    }

    static bool CreateSessionFile(const std::string& deviceKey, const std::string& version)
    {
        return FileSystemUtils::createFileWithContent(FileSystemUtils::composePath(".fw-session_" + deviceKey, "./"),
                                                      version);
    }

    FirmwareInstallerMock& GetFirmwareInstallReference() const
    {
        return dynamic_cast<FirmwareInstallerMock&>(*service->m_firmwareInstaller);
    }

    FirmwareParametersListenerMock& GetFirmwareParametersListenerReference() const
    {
        return dynamic_cast<FirmwareParametersListenerMock&>(*service->m_firmwareParametersListener);
    }

    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    const std::string TEST_FILE = "test.file";

    const std::string DEVICE_KEY = "TestDevice";

    const std::string FIRMWARE_VERSION_1 = "1.0.0";

    const std::string FIRMWARE_VERSION_2 = "1.0.1";

    std::unique_ptr<FirmwareUpdateService> service;

    ConnectivityServiceMock connectivityServiceMock;

    OutboundMessageHandlerMock outboundMessageHandlerMock;

    OutboundRetryMessageHandlerMock outboundRetryMessageHandler{outboundMessageHandlerMock};

    DataProtocolMock dataProtocolMock;

    PersistenceMock persistenceMock;

    DataServiceMock dataServiceMock{
      dataProtocolMock, persistenceMock, connectivityServiceMock, outboundRetryMessageHandler, {}, {}, {}};

    FileManagementProtocolMock fileManagementProtocolMock;

    std::shared_ptr<FileManagementServiceMock> fileManagementServiceMock;

    FirmwareUpdateProtocolMock firmwareUpdateProtocolMock;

    std::unique_ptr<FirmwareInstallerMock> firmwareInstallerMock;

    std::unique_ptr<FirmwareParametersListenerMock> firmwareParametersListenerMock;
};

TEST_F(FirmwareUpdateServiceTests, ProtocolCheck)
{
    CreateServiceWithInstaller();
    EXPECT_EQ(&firmwareUpdateProtocolMock, &(service->getProtocol()));
}

TEST_F(FirmwareUpdateServiceTests, InstallerCheckParams)
{
    CreateServiceWithInstaller();
    EXPECT_TRUE(service->isInstaller());
    EXPECT_FALSE(service->isParameterListener());
    EXPECT_CALL(GetFirmwareInstallReference(), getFirmwareVersion).WillOnce(Return(FIRMWARE_VERSION_1));
    EXPECT_EQ(service->getVersionForDevice(DEVICE_KEY), FIRMWARE_VERSION_1);
}

TEST_F(FirmwareUpdateServiceTests, ParameterListenerCheckParams)
{
    CreateServiceWithParameterListener();
    EXPECT_FALSE(service->isInstaller());
    EXPECT_TRUE(service->isParameterListener());
    EXPECT_CALL(GetFirmwareParametersListenerReference(), getFirmwareVersion).WillOnce(Return(FIRMWARE_VERSION_2));
    EXPECT_EQ(service->getVersionForDevice(DEVICE_KEY), FIRMWARE_VERSION_2);
}

TEST_F(FirmwareUpdateServiceTests, SessionFileCheck)
{
    CreateServiceWithInstaller();

    // Check that it does not exist
    const auto filePath = "./.fw-session_" + DEVICE_KEY;
    ASSERT_FALSE(FileSystemUtils::isFilePresent(filePath));

    // Create the session file
    ASSERT_TRUE(service->storeSessionFile(DEVICE_KEY, FIRMWARE_VERSION_1));
    ASSERT_TRUE(FileSystemUtils::isFilePresent(filePath));

    // And now delete it
    ASSERT_NO_FATAL_FAILURE(service->deleteSessionFile(DEVICE_KEY));
    ASSERT_FALSE(FileSystemUtils::isFilePresent(filePath));
}

TEST_F(FirmwareUpdateServiceTests, QueueStatusMessageFailsToParse)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->queueStatusMessage(DEVICE_KEY, FirmwareUpdateStatus::INSTALLING));
    EXPECT_TRUE(service->getQueue().empty());
}

TEST_F(FirmwareUpdateServiceTests, QueueStatusMessageHappyFlow)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage)
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    ASSERT_NO_FATAL_FAILURE(service->queueStatusMessage(DEVICE_KEY, FirmwareUpdateStatus::INSTALLING));
    EXPECT_FALSE(service->getQueue().empty());
}

TEST_F(FirmwareUpdateServiceTests, PublishStatusMessageFailsToParse)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->sendStatusMessage(DEVICE_KEY, FirmwareUpdateStatus::INSTALLING));
}

TEST_F(FirmwareUpdateServiceTests, PublishStatusMessageHappyFlow)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage)
      .WillOnce(Return(ByMove(std::unique_ptr<wolkabout::Message>{new wolkabout::Message{"", ""}})));
    EXPECT_CALL(connectivityServiceMock, publish).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->sendStatusMessage(DEVICE_KEY, FirmwareUpdateStatus::INSTALLING));
}

TEST_F(FirmwareUpdateServiceTests, AbortAFirmwareInstallation)
{
    CreateServiceWithInstaller();
    service->m_installation.emplace(DEVICE_KEY, true);
    EXPECT_CALL(GetFirmwareInstallReference(), abortFirmwareInstall(DEVICE_KEY)).Times(1);
    ASSERT_NO_FATAL_FAILURE(service->onFirmwareAbort(DEVICE_KEY, FirmwareUpdateAbortMessage{}));
}

TEST_F(FirmwareUpdateServiceTests, LoadStateNoFirmwareInstaller)
{
    CreateServiceWithParameterListener();
    ASSERT_TRUE(CreateSessionFile(DEVICE_KEY, FIRMWARE_VERSION_1));
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->loadState(DEVICE_KEY));
}

TEST_F(FirmwareUpdateServiceTests, LoadStateHappyFlowSameVersion)
{
    CreateServiceWithInstaller();
    ASSERT_TRUE(CreateSessionFile(DEVICE_KEY, FIRMWARE_VERSION_1));
    EXPECT_CALL(GetFirmwareInstallReference(), getFirmwareVersion).WillOnce(Return(FIRMWARE_VERSION_1));
    EXPECT_CALL(GetFirmwareInstallReference(), wasFirmwareInstallSuccessful)
      .WillOnce([&](const std::string& deviceKey, const std::string& oldContent) {
          return GetFirmwareInstallReference().FirmwareInstaller::wasFirmwareInstallSuccessful(deviceKey, oldContent);
      });
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->loadState(DEVICE_KEY));
}

TEST_F(FirmwareUpdateServiceTests, LoadStateHappyFlowNewVersion)
{
    CreateServiceWithInstaller();
    ASSERT_TRUE(CreateSessionFile(DEVICE_KEY, FIRMWARE_VERSION_1));
    EXPECT_CALL(GetFirmwareInstallReference(), getFirmwareVersion).WillOnce(Return(FIRMWARE_VERSION_2));
    EXPECT_CALL(GetFirmwareInstallReference(), wasFirmwareInstallSuccessful)
      .WillOnce([&](const std::string& deviceKey, const std::string& oldContent) {
          return GetFirmwareInstallReference().FirmwareInstaller::wasFirmwareInstallSuccessful(deviceKey, oldContent);
      });
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->loadState(DEVICE_KEY));
}

TEST_F(FirmwareUpdateServiceTests, ObtainParametersNoListener)
{
    CreateServiceWithInstaller();
    ASSERT_NO_FATAL_FAILURE(service->obtainParametersAndAnnounce(DEVICE_KEY));
}

TEST_F(FirmwareUpdateServiceTests, ObtainParametersHappyFlow)
{
    CreateServiceWithParameterListener();
    EXPECT_CALL(GetFirmwareParametersListenerReference(), receiveParameters("TestRepository", "TestTime")).Times(1);
    EXPECT_CALL(dataServiceMock, synchronizeParameters)
      .WillOnce([&](const std::string&, const std::vector<ParameterName>&,
                    std::function<void(std::vector<Parameter>)> callback) {
          callback({{ParameterName::FIRMWARE_UPDATE_REPOSITORY, "TestRepository"},
                    {ParameterName::FIRMWARE_UPDATE_CHECK_TIME, "TestTime"}});
          return true;
      });
    ASSERT_NO_FATAL_FAILURE(service->obtainParametersAndAnnounce(DEVICE_KEY));
}

TEST_F(FirmwareUpdateServiceTests, OnFirmwareInstallAlreadyOngoing)
{
    CreateServiceWithInstaller();
    service->m_installation.emplace(DEVICE_KEY, true);
    ASSERT_NO_FATAL_FAILURE(service->onFirmwareInstall(DEVICE_KEY, FirmwareUpdateInstallMessage{TEST_FILE}));
}

TEST_F(FirmwareUpdateServiceTests, OnFirmwareInstallOtherStatuses)
{
    CreateServiceWithInstaller();
    const auto statuses = {InstallResponse::FAILED_TO_INSTALL, InstallResponse::NO_FILE, InstallResponse::INSTALLED};
    for (const auto& status : statuses)
    {
        EXPECT_CALL(GetFirmwareInstallReference(), installFirmware).WillOnce(Return(status));
        EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage)
          .WillOnce(Return(ByMove(nullptr)))
          .WillOnce(Return(ByMove(nullptr)));
        ASSERT_NO_FATAL_FAILURE(service->onFirmwareInstall(DEVICE_KEY, FirmwareUpdateInstallMessage{TEST_FILE}));
    }
}

TEST_F(FirmwareUpdateServiceTests, OnFirmwareInstallWillInstallFailsToStoreSessionFile)
{
    CreateServiceWithInstaller("./non-existing-dir/");
    EXPECT_CALL(GetFirmwareInstallReference(), installFirmware).WillOnce(Return(InstallResponse::WILL_INSTALL));
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage)
      .WillOnce(Return(ByMove(nullptr)))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->onFirmwareInstall(DEVICE_KEY, FirmwareUpdateInstallMessage{TEST_FILE}));
}

TEST_F(FirmwareUpdateServiceTests, OnFirmwareInstallWillInstallStoresSessionFile)
{
    CreateServiceWithInstaller();
    ASSERT_FALSE(FileSystemUtils::isFilePresent("./.fw-session_" + DEVICE_KEY));
    EXPECT_CALL(GetFirmwareInstallReference(), installFirmware).WillOnce(Return(InstallResponse::WILL_INSTALL));
    EXPECT_CALL(firmwareUpdateProtocolMock, makeOutboundMessage)
      .WillOnce(Return(ByMove(nullptr)))
      .WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->onFirmwareInstall(DEVICE_KEY, FirmwareUpdateInstallMessage{TEST_FILE}));
    ASSERT_TRUE(FileSystemUtils::isFilePresent("./.fw-session_" + DEVICE_KEY));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedNullMessage)
{
    CreateServiceWithInstaller();
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(nullptr));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedNoInstaller)
{
    CreateServiceWithParameterListener();
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedInvalidType)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, getMessageType).WillOnce(Return(MessageType::UNKNOWN));
    EXPECT_CALL(firmwareUpdateProtocolMock, getDeviceKey).WillOnce(Return(""));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedInstallationFailsToParse)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, getMessageType).WillOnce(Return(MessageType::FIRMWARE_UPDATE_INSTALL));
    EXPECT_CALL(firmwareUpdateProtocolMock, getDeviceKey).WillOnce(Return(""));
    EXPECT_CALL(firmwareUpdateProtocolMock, parseFirmwareUpdateInstall).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedInstallationHappyFlow)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, getMessageType).WillOnce(Return(MessageType::FIRMWARE_UPDATE_INSTALL));
    EXPECT_CALL(firmwareUpdateProtocolMock, getDeviceKey).WillOnce(Return(""));
    EXPECT_CALL(firmwareUpdateProtocolMock, parseFirmwareUpdateInstall)
      .WillOnce(
        Return(ByMove(std::unique_ptr<FirmwareUpdateInstallMessage>{new FirmwareUpdateInstallMessage{TEST_FILE}})));
    service->m_installation[DEVICE_KEY] = true;
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedAbortFailsToParse)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, getMessageType).WillOnce(Return(MessageType::FIRMWARE_UPDATE_ABORT));
    EXPECT_CALL(firmwareUpdateProtocolMock, getDeviceKey).WillOnce(Return(""));
    EXPECT_CALL(firmwareUpdateProtocolMock, parseFirmwareUpdateAbort).WillOnce(Return(ByMove(nullptr)));
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

TEST_F(FirmwareUpdateServiceTests, MessageReceivedAbortHappyFlow)
{
    CreateServiceWithInstaller();
    EXPECT_CALL(firmwareUpdateProtocolMock, getMessageType).WillOnce(Return(MessageType::FIRMWARE_UPDATE_ABORT));
    EXPECT_CALL(firmwareUpdateProtocolMock, getDeviceKey).WillOnce(Return(""));
    EXPECT_CALL(firmwareUpdateProtocolMock, parseFirmwareUpdateAbort)
      .WillOnce(Return(ByMove(std::unique_ptr<FirmwareUpdateAbortMessage>{new FirmwareUpdateAbortMessage})));
    service->m_installation[DEVICE_KEY] = false;
    ASSERT_NO_FATAL_FAILURE(service->messageReceived(std::make_shared<wolkabout::Message>("", "")));
}

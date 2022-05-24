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

#include <any>
#include <sstream>

#define private public
#define protected public
#include "wolk/WolkBuilder.h"
#include "wolk/WolkMulti.h"
#include "wolk/WolkSingle.h"
#undef private
#undef protected

#include "core/utilities/Logger.h"
#include "tests/mocks/DataProtocolMock.h"
#include "tests/mocks/ErrorProtocolMock.h"
#include "tests/mocks/FeedUpdateHandlerMock.h"
#include "tests/mocks/FileDownloaderMock.h"
#include "tests/mocks/FileListenerMock.h"
#include "tests/mocks/FirmwareInstallerMock.h"
#include "tests/mocks/FirmwareParametersListenerMock.h"
#include "tests/mocks/ParameterHandlerMock.h"
#include "tests/mocks/PersistenceMock.h"
#include "tests/mocks/PlatformStatusListenerMock.h"
#include "tests/mocks/RegistrationProtocolMock.h"

#include <gmock/gmock.h>

using namespace wolkabout::connect;
using namespace ::testing;

class WolkBuilderTests : public ::testing::Test
{
public:
    static void SetUpTestCase() { Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE); }

    void SetUp() override
    {
        feedUpdateHandlerMock = std::make_shared<NiceMock<FeedUpdateHandlerMock>>();
        parameterHandlerMock = std::make_shared<NiceMock<ParameterHandlerMock>>();
        persistenceMock = std::unique_ptr<PersistenceMock>{new NiceMock<PersistenceMock>};
        dataProtocolMock = std::unique_ptr<DataProtocolMock>{new DataProtocolMock};
        errorProtocolMock = std::unique_ptr<ErrorProtocolMock>{new ErrorProtocolMock};
        fileDownloaderMock = std::unique_ptr<FileDownloaderMock>{new FileDownloaderMock};
        fileListenerMock = std::make_shared<NiceMock<FileListenerMock>>();
        firmwareInstallerMock = std::unique_ptr<FirmwareInstallerMock>{new NiceMock<FirmwareInstallerMock>};
        firmwareParameterListenerMock =
          std::unique_ptr<FirmwareParametersListenerMock>{new NiceMock<FirmwareParametersListenerMock>};
        platformStatusListenerMock =
          std::unique_ptr<PlatformStatusListenerMock>{new NiceMock<PlatformStatusListenerMock>};
    }

    const Device device{"TestDevice", "TestPassword", OutboundDataMode::PUSH};

    const std::vector<Device> devices{Device{"TestDevice1", "", OutboundDataMode::PUSH},
                                      {"TestDevice2", "", OutboundDataMode::PUSH}};

    const std::string hostPath = "Host!";

    const std::string hostCaCrt = "CaCrt!";

    std::shared_ptr<FeedUpdateHandlerMock> feedUpdateHandlerMock;

    std::shared_ptr<ParameterHandlerMock> parameterHandlerMock;

    std::unique_ptr<PersistenceMock> persistenceMock;

    std::unique_ptr<DataProtocolMock> dataProtocolMock;

    const std::chrono::seconds errorRetainTime{10};

    std::unique_ptr<ErrorProtocolMock> errorProtocolMock;

    const std::string fileDownloadLocation = "./files";

    const std::uint64_t maxPacketSize = 1024;

    std::unique_ptr<FileDownloaderMock> fileDownloaderMock;

    std::shared_ptr<FileListenerMock> fileListenerMock;

    std::unique_ptr<FirmwareInstallerMock> firmwareInstallerMock;

    std::unique_ptr<FirmwareParametersListenerMock> firmwareParameterListenerMock;

    std::unique_ptr<PlatformStatusListenerMock> platformStatusListenerMock;
};

TEST_F(WolkBuilderTests, GetDevices)
{
    auto builder = WolkBuilder{};
    EXPECT_TRUE(builder.getDevices().empty());
}

TEST_F(WolkBuilderTests, BuildSingleButMultipleDevices)
{
    ASSERT_THROW(
      ([] {
          WolkBuilder{{{"", "", OutboundDataMode::PUSH}, {"", "", OutboundDataMode::PUSH}}}.buildWolkSingle();
      }()),
      std::runtime_error);
}

TEST_F(WolkBuilderTests, BuildSingleEmptyFields)
{
    ASSERT_THROW(([] { WolkBuilder{{"", "", OutboundDataMode::PUSH}}.buildWolkSingle(); }()), std::runtime_error);
    ASSERT_THROW(([] {
                     WolkBuilder{{"SOME_DEVICE", "", OutboundDataMode::PUSH}}.buildWolkSingle();
                 }()),
                 std::runtime_error);
}

TEST_F(WolkBuilderTests, BuildMultiButDeviceHasEmptyKey)
{
    ASSERT_THROW(([] { WolkBuilder{{{"", "", OutboundDataMode::PUSH}}}.buildWolkMulti(); }()), std::runtime_error);
}

TEST_F(WolkBuilderTests, BuildInvalidType)
{
    ASSERT_THROW(([] { WolkBuilder{{}}.build(WolkInterfaceType::Gateway); }()), std::runtime_error);
}

TEST_F(WolkBuilderTests, FullSingleExample)
{
    auto wolk = std::unique_ptr<WolkSingle>{};
    ASSERT_NO_FATAL_FAILURE([&] {
        wolk = WolkBuilder{device}
                 .host(hostPath)
                 .caCertPath(hostCaCrt)
                 .feedUpdateHandler([](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) {})
                 .feedUpdateHandler(feedUpdateHandlerMock)
                 .parameterHandler([](const std::string&, const std::vector<Parameter>&) {})
                 .parameterHandler(parameterHandlerMock)
                 .withPersistence(std::move(persistenceMock))
                 .withDataProtocol(std::move(dataProtocolMock))
                 .withErrorProtocol(errorRetainTime, std::move(errorProtocolMock))
                 .withFileTransfer(fileDownloadLocation, maxPacketSize)
                 .withFileURLDownload(fileDownloadLocation, std::move(fileDownloaderMock), true, maxPacketSize)
                 .withFileListener(fileListenerMock)
                 .withFirmwareUpdate(std::move(firmwareInstallerMock), fileDownloadLocation)
                 .buildWolkSingle();
    }());
    ASSERT_NE(wolk, nullptr);

    // Call some methods
    ASSERT_NO_FATAL_FAILURE(wolk->m_connectivityService->m_onConnectionLost());
    ASSERT_NO_FATAL_FAILURE(wolk->m_dataService->m_feedUpdateHandler("", {}));
    ASSERT_NO_FATAL_FAILURE(wolk->m_dataService->m_parameterSyncHandler("", {}));
    ASSERT_NO_FATAL_FAILURE(wolk->m_dataService->m_detailsSyncHandler("", {"F1"}, {"A1"}));
}

TEST_F(WolkBuilderTests, FullMultiExample)
{
    auto wolk = std::unique_ptr<WolkMulti>{};
    ASSERT_NO_FATAL_FAILURE([&] {
        wolk = WolkBuilder{devices}
                 .host(hostPath)
                 .caCertPath(hostCaCrt)
                 .feedUpdateHandler([](const std::string&, const std::map<std::uint64_t, std::vector<Reading>>&) {})
                 .feedUpdateHandler(feedUpdateHandlerMock)
                 .parameterHandler([](const std::string&, const std::vector<Parameter>&) {})
                 .parameterHandler(parameterHandlerMock)
                 .withPersistence(std::move(persistenceMock))
                 .withDataProtocol(std::move(dataProtocolMock))
                 .withErrorProtocol(errorRetainTime, std::move(errorProtocolMock))
                 .withFileTransfer(fileDownloadLocation, maxPacketSize)
                 .withFileURLDownload(fileDownloadLocation, std::move(fileDownloaderMock), true, maxPacketSize)
                 .withFileListener(fileListenerMock)
                 .withFirmwareUpdate(std::move(firmwareParameterListenerMock), fileDownloadLocation)
                 .withPlatformStatus(std::move(platformStatusListenerMock))
                 .withRegistration()
                 .buildWolkMulti();
    }());
    ASSERT_NE(wolk, nullptr);

    // Call some methods
    ASSERT_NO_FATAL_FAILURE(wolk->m_connectivityService->m_onConnectionLost());
    ASSERT_NO_FATAL_FAILURE(wolk->m_dataService->m_feedUpdateHandler("", {}));
    ASSERT_NO_FATAL_FAILURE(wolk->m_dataService->m_parameterSyncHandler("", {}));
}

# Copyright 2021 WolkAbout Technology s.r.o.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.5)
project(WolkAboutConnector C CXX)

set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS 1)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}    -Wall -Wextra -pedantic                                                    \
                                            -pedantic-errors -Wcast-align                                              \
                                            -Wcast-qual -Wconversion                                                   \
                                            -Wdisabled-optimization                                                    \
                                            -Wfloat-equal -Wformat=2                                                   \
                                            -Wformat-nonliteral -Wformat-security                                      \
                                            -Wformat-y2k -Winit-self                                                   \
                                            -Wmissing-field-initializers -Wmissing-format-attribute                    \
                                            -Wmissing-include-dirs -Wmissing-noreturn                                  \
                                            -Wpointer-arith                                                            \
                                            -Wno-packed  -Wno-padded -Wredundant-decls                                 \
                                            -Wshadow -Wstack-protector                                                 \
                                            -Wstrict-aliasing=2                                                        \
                                            -Wunreachable-code -Wno-unused                                             \
                                            -Wvariadic-macros                                                          \
                                            -Wwrite-strings")

# Setup the options for tests
OPTION(BUILD_GTEST "Build the library with gtest" ON)
OPTION(BUILD_TESTS "Build the unit tests for the library" ON)
if (${BUILD_TESTS})
    if (NOT ${BUILD_GTEST})
        message(FATAL "Cannot Build Tests with BUILD_GTEST set to false.")
    endif ()

    # Setup test coverage if it is necessary
    OPTION(TEST_COVERAGE "Build the library for test coverage" ON)
    if (${TEST_COVERAGE})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -g")
    endif ()
endif ()

# Setup the option for optional Poco file downloader
OPTION(BUILD_POCO_HTTP_DOWNLOADER "Build the optional `FileDownloader` implementation that uses Poco" ON)

# Setup the options for the examples
OPTION(BUILD_EXAMPLES "Build the examples/runtimes for testing" ON)

# Check if the paths for output are set, if not, we can set them ourselves
if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif ()

if (NOT DEFINED CMAKE_LIBRARY_INCLUDE_DIRECTORY)
    set(CMAKE_LIBRARY_INCLUDE_DIRECTORY "${CMAKE_BINARY_DIR}/include")
endif ()

if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif ()

if (NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}/build")
endif ()

link_directories(${CMAKE_PREFIX_PATH}/lib)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
include_directories(SYSTEM ${CMAKE_PREFIX_PATH}/include)

find_package(Threads REQUIRED)

# WolkAbout c++ SDK
option(BUILD_POCO "" ${BUILD_POCO_HTTP_DOWNLOADER})
option(POCO_BUILD_NET "" ${BUILD_POCO_HTTP_DOWNLOADER})
option(BUILD_AWS_LOG_UPLOADER "" OFF)
add_subdirectory(WolkSDK-Cpp)

# WolkAbout c++ Connector
set(LIB_SOURCE_FILES wolk/api/FirmwareInstaller.cpp
        wolk/connectivity/mqtt/WolkPahoMqttClient.cpp
        wolk/service/data/DataService.cpp
        wolk/service/error/ErrorService.cpp
        wolk/service/file_management/FileManagementService.cpp
        wolk/service/file_management/FileTransferSession.cpp
        wolk/service/firmware_update/FirmwareUpdateService.cpp
        wolk/service/platform_status/PlatformStatusService.cpp
        wolk/service/registration_service/RegistrationService.cpp
        wolk/InboundPlatformMessageHandler.cpp
        wolk/WolkBuilder.cpp
        wolk/WolkInterface.cpp
        wolk/WolkMulti.cpp
        wolk/WolkSingle.cpp)
set(LIB_HEADER_FILES wolk/api/FeedUpdateHandler.h
        wolk/api/FileListener.h
        wolk/api/FirmwareInstaller.h
        wolk/api/FirmwareParametersListener.h
        wolk/api/ParameterHandler.h
        wolk/api/PlatformStatusListener.h
        wolk/connectivity/mqtt/WolkPahoMqttClient.h
        wolk/service/data/DataService.h
        wolk/service/error/ErrorService.h
        wolk/service/file_management/FileDownloader.h
        wolk/service/file_management/FileManagementService.h
        wolk/service/file_management/FileTransferSession.h
        wolk/service/firmware_update/FirmwareUpdateService.h
        wolk/service/platform_status/PlatformStatusService.h
        wolk/service/registration_service/RegistrationService.h
        wolk/InboundPlatformMessageHandler.h
        wolk/Version.h
        wolk/WolkBuilder.h
        wolk/WolkInterface.h
        wolk/WolkInterfaceType.h
        wolk/WolkMulti.h
        wolk/WolkSingle.h)

file(COPY wolk/ DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk PATTERN *.cpp EXCLUDE PATTERN "poco/HTTPFileDownloader.h" EXCLUDE)

if (${BUILD_POCO_HTTP_DOWNLOADER})
    set(LIB_SOURCE_FILES ${LIB_SOURCE_FILES} wolk/service/file_management/poco/HTTPFileDownloader.cpp)
    set(LIB_HEADER_FILES ${LIB_HEADER_FILES} wolk/service/file_management/poco/HTTPFileDownloader.h)
    file(COPY wolk/service/file_management/poco/HTTPFileDownloader.h DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk/service/file_management/poco)
endif ()

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCE_FILES} ${LIB_HEADER_FILES})
target_link_libraries(${PROJECT_NAME} WolkAboutCore Threads::Threads)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_LIBRARY_INCLUDE_DIRECTORY} ${CMAKE_PREFIX_PATH}/include)
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")

if (${BUILD_POCO_HTTP_DOWNLOADER})
    target_link_libraries(${PROJECT_NAME} PocoFoundation PocoCrypto PocoJSON PocoNet PocoNetSSL PocoUtil)
    add_dependencies(${PROJECT_NAME} libpoco)
endif ()

# Tests
if (${BUILD_TESTS})
    set(TEST_SOURCE_FILES
            tests/DataServiceTests.cpp
            tests/FileManagementServiceTests.cpp
            tests/FileTransferSessionTests.cpp
            tests/FirmwareUpdateServiceTests.cpp
            tests/InboundPlatformMessageHandlerTests.cpp
            tests/WolkBuilderTests.cpp
            tests/WolkTests.cpp)
    set(TEST_HEADER_FILES
            tests/mocks/ConnectivityServiceMock.h
            tests/mocks/DataProtocolMock.h
            tests/mocks/DataServiceMock.h
            tests/mocks/FeedUpdateHandlerMock.h
            tests/mocks/FileManagementProtocolMock.h
            tests/mocks/FileManagementServiceMock.h
            tests/mocks/FirmwareUpdateProtocolMock.h
            tests/mocks/InboundMessageHandlerMock.h
            tests/mocks/MessageListenerMock.h
            tests/mocks/ParameterHandlerMock.h
            tests/mocks/PersistenceMock.h
            tests/mocks/ProtocolMock.h
            tests/mocks/WolkPahoMqttClientMock.h)

    add_executable(${PROJECT_NAME}Tests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})
    target_link_libraries(${PROJECT_NAME}Tests ${PROJECT_NAME} gtest_main gtest gmock_main gmock)
    target_include_directories(${PROJECT_NAME}Tests PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_LIBRARY_INCLUDE_DIRECTORY} ${CMAKE_PREFIX_PATH}/include)
    set_target_properties(${PROJECT_NAME}Tests PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    set_target_properties(${PROJECT_NAME}Tests PROPERTIES EXCLUDE_FROM_ALL TRUE)
    add_dependencies(${PROJECT_NAME}Tests ${PROJECT_NAME} libgtest)

    add_custom_target(tests ${PROJECT_NAME}Tests)
    add_test(gtest ${PROJECT_NAME}Tests)
endif ()

if (${BUILD_EXAMPLES})
    # Full example
    set(FULL_EXAMPLE_SOURCE_FILES examples/full_feature/Application.cpp)

    add_executable(full_example ${FULL_EXAMPLE_SOURCE_FILES})
    target_link_libraries(full_example ${PROJECT_NAME})
    set_target_properties(full_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Pull example
    set(PULL_EXAMPLE_SOURCE_FILES examples/pull/Application.cpp)

    add_executable(pull_example ${PULL_EXAMPLE_SOURCE_FILES})
    target_link_libraries(pull_example ${PROJECT_NAME})
    set_target_properties(pull_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Register Feed and Attribute example
    set(REGISTER_EXAMPLE_SOURCE_FILES examples/register_feed_and_attribute/Application.cpp)

    add_executable(register_example ${REGISTER_EXAMPLE_SOURCE_FILES})
    target_link_libraries(register_example ${PROJECT_NAME})
    set_target_properties(register_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Simple example
    set(SIMPLE_EXAMPLE_SOURCE_FILES examples/simple/Application.cpp)

    add_executable(simple_example ${SIMPLE_EXAMPLE_SOURCE_FILES})
    target_link_libraries(simple_example ${PROJECT_NAME})
    set_target_properties(simple_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Subdevices example
    set(SUBDEVICES_EXAMPLE_SOURCE_FILES examples/subdevices/Application.cpp)

    add_executable(subdevices_example ${SUBDEVICES_EXAMPLE_SOURCE_FILES})
    target_link_libraries(subdevices_example ${PROJECT_NAME})
    set_target_properties(subdevices_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif ()

# Create the install rule
include(GNUInstallDirs)
install(DIRECTORY ${CMAKE_LIBRARY_INCLUDE_DIRECTORY} DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN *.h)
install(DIRECTORY ${CMAKE_PREFIX_PATH}/include DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN *.h)
install(DIRECTORY ${CMAKE_PREFIX_PATH}/lib/ DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# Add the format target
add_custom_target(format
        COMMAND "clang-format" -i -sort-includes -style=file ${LIB_HEADER_FILES} ${LIB_SOURCE_FILES}
        ${TEST_HEADER_FILES} ${TEST_SOURCE_FILES} ${FULL_EXAMPLE_SOURCE_FILES} ${PULL_EXAMPLE_SOURCE_FILES}
        ${REGISTER_EXAMPLE_SOURCE_FILES} ${SIMPLE_EXAMPLE_SOURCE_FILES}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "[Formatting source code]"
        VERBATIM)

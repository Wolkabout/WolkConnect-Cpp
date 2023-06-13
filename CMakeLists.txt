# Copyright 2022 Wolkabout Technology s.r.o.
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

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}     -Wall -Wextra -pedantic                                                    \
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

# Setup the option for optional apt/systemd firmware updaters
OPTION(BUILD_APT_SYSTEMD_FIRMWARE_UPDATER "Build the optional apt/systemd firmware updaters" OFF)

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

# Usually we just need PThreads
find_package(Threads REQUIRED)

# But if we want the apt/systemd firmware updater, we need GLib
if (${BUILD_APT_SYSTEMD_FIRMWARE_UPDATER})
    find_package(PkgConfig REQUIRED)
    PKG_CHECK_MODULES(GLIB glib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})
    link_directories(${GLIB_LIBRARY_DIRS})
    add_definitions(${GLIB_CFLAGS_OTHER})

    # Stop if GLib is not found
    if (NOT GLIB_FOUND)
        message(FATAL_ERROR "CMake was not able to find GLib on your system.
                         Install using `apt install libglib2.0-dev`.
                         Stopping.")
    endif ()

    # Link in GIO
    PKG_CHECK_MODULES(GIO gio-2.0)
    link_directories(${GIO_LIBRARY_DIRS})
    add_definitions(${GIO_CFLAGS_OTHER})

    # Stop if GIO is not found
    if (NOT GIO_FOUND)
        message(FATAL_ERROR "CMake was not able to find GIO on your system.
                         Install using `apt install libglib2.0-dev`.
                         Stopping.")
    endif ()
endif ()

# WolkAbout c++ SDK
option(BUILD_POCO "" ${BUILD_POCO_HTTP_DOWNLOADER})
option(POCO_BUILD_NET "" ${BUILD_POCO_HTTP_DOWNLOADER})
option(BUILD_AWS_LOG_UPLOADER "" OFF)
add_subdirectory(WolkSDK-Cpp)

# Define the version of the library
set(WOLKCONNECT_VERSION_MAJOR 4)
set(WOLKCONNECT_VERSION_MINOR 3)
set(WOLKCONNECT_VERSION_PATCH 0)
set(WOLKCONNECT_VERSION_SUFFIX)

set(WOLKCONNECT_VERSION_STRING "${WOLKCONNECT_VERSION_MAJOR}.${WOLKCONNECT_VERSION_MINOR}.${WOLKCONNECT_VERSION_PATCH}")
if (NOT "${WOLKCONNECT_VERSION_SUFFIX}" STREQUAL "")
    set(WOLKCONNECT_VERSION_STRING "${WOLKCONNECT_VERSION_STRING}-${WOLKCONNECT_VERSION_SUFFIX}")
endif ()

# WolkAbout c++ Connector
set(LIB_SOURCE_FILES wolk/api/FirmwareInstaller.cpp
        wolk/service/data/DataService.cpp
        wolk/service/error/ErrorService.cpp
        wolk/service/file_management/FileManagementService.cpp
        wolk/service/file_management/FileTransferSession.cpp
        wolk/service/firmware_update/FirmwareUpdateService.cpp
        wolk/service/platform_status/PlatformStatusService.cpp
        wolk/service/registration_service/RegistrationService.cpp
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
        wolk/service/data/DataService.h
        wolk/service/error/ErrorService.h
        wolk/service/file_management/FileDownloader.h
        wolk/service/file_management/FileManagementService.h
        wolk/service/file_management/FileTransferSession.h
        wolk/service/firmware_update/FirmwareUpdateService.h
        wolk/service/platform_status/PlatformStatusService.h
        wolk/service/registration_service/RegistrationService.h
        wolk/Version.h
        wolk/WolkBuilder.h
        wolk/WolkInterface.h
        wolk/WolkInterfaceType.h
        wolk/WolkMulti.h
        wolk/WolkSingle.h)

file(COPY wolk/ DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk PATTERN *.cpp EXCLUDE PATTERN "poco/HTTPFileDownloader.h" EXCLUDE)

if (${BUILD_APT_SYSTEMD_FIRMWARE_UPDATER})
    set(LIB_SOURCE_FILES ${LIB_SOURCE_FILES} wolk/service/firmware_update/debian/DebianPackageInstaller.cpp
            wolk/service/firmware_update/debian/GenericDBusInterface.cpp
            wolk/service/firmware_update/debian/apt/APTPackageInstaller.cpp
            wolk/service/firmware_update/debian/systemd/SystemdServiceInterface.cpp)
    set(LIB_HEADER_FILES ${LIB_HEADER_FILES} wolk/service/firmware_update/debian/DebianPackageInstaller.h
            wolk/service/firmware_update/debian/GenericDBusInterface.h
            wolk/service/firmware_update/debian/apt/APTPackageInstaller.h
            wolk/service/firmware_update/debian/systemd/SystemdServiceInterface.h)
    file(COPY wolk/service/firmware_update/debian/GenericDBusInterface.h DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk/service/firmware_update/apt)
    file(COPY wolk/service/firmware_update/debian/DebianPackageInstaller.h DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk/service/firmware_update/apt)
    file(COPY wolk/service/firmware_update/debian/apt/APTPackageInstaller.h DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk/service/firmware_update/apt)
    file(COPY wolk/service/firmware_update/debian/systemd/SystemdServiceInterface.h DESTINATION ${CMAKE_LIBRARY_INCLUDE_DIRECTORY}/wolk/service/firmware_update/apt)
endif ()

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCE_FILES} ${LIB_HEADER_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC SDKCore Threads::Threads)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_BINARY_DIR}/include)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${WOLKCONNECT_VERSION_STRING} SOVERSION ${WOLKCONNECT_VERSION_MAJOR}.${WOLKCONNECT_VERSION_MINOR}.${WOLKCONNECT_VERSION_PATCH})

if (${BUILD_APT_SYSTEMD_FIRMWARE_UPDATER})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${GLIB_LIBRARIES} ${GIO_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${GLIB_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})
endif ()

# Tests
if (${BUILD_TESTS})
    set(TEST_SOURCE_FILES
            tests/DataServiceTests.cpp
            tests/ErrorServiceTests.cpp
            tests/FileManagementServiceTests.cpp
            tests/FileTransferSessionTests.cpp
            tests/FirmwareUpdateServiceTests.cpp
            tests/InboundPlatformMessageHandlerTests.cpp
            tests/PlatformStatusServiceTests.cpp
            tests/RegistrationServiceTests.cpp
            tests/WolkBuilderTests.cpp
            tests/WolkMultiTests.cpp
            tests/WolkSingleTests.cpp)
    set(TEST_HEADER_FILES
            tests/mocks/DataServiceMock.h
            tests/mocks/ErrorServiceMock.h
            tests/mocks/FeedUpdateHandlerMock.h
            tests/mocks/FileDownloaderMock.h
            tests/mocks/FileListenerMock.h
            tests/mocks/FileManagementServiceMock.h
            tests/mocks/FileTransferSessionMock.h
            tests/mocks/FirmwareInstallerMock.h
            tests/mocks/FirmwareParametersListenerMock.h
            tests/mocks/FirmwareUpdateServiceMock.h
            tests/mocks/ParameterHandlerMock.h
            tests/mocks/PlatformStatusListenerMock.h
            tests/mocks/RegistrationServiceMock.h)

    enable_testing()
    add_executable(${PROJECT_NAME}Tests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})
    target_link_libraries(${PROJECT_NAME}Tests ${PROJECT_NAME} gtest_main gtest gmock_main gmock)
    target_include_directories(${PROJECT_NAME}Tests PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_LIBRARY_INCLUDE_DIRECTORY} ${CMAKE_BINARY_DIR}/include WolkSDK-Cpp)
    set_target_properties(${PROJECT_NAME}Tests PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    add_test(NAME "WolkConnect-Cpp_Tests" COMMAND ${PROJECT_NAME}Tests)
endif ()

if (${BUILD_EXAMPLES})
    # Full example
    set(FULL_EXAMPLE_SOURCE_FILES examples/full_feature/Application.cpp)

    add_executable(full_example ${FULL_EXAMPLE_SOURCE_FILES})
    target_link_libraries(full_example ${PROJECT_NAME})
    target_include_directories(full_example PRIVATE ${PROJECT_SOURCE_DIR})
    set_target_properties(full_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Pull example
    set(PULL_EXAMPLE_SOURCE_FILES examples/pull/Application.cpp)

    add_executable(pull_example ${PULL_EXAMPLE_SOURCE_FILES})
    target_link_libraries(pull_example ${PROJECT_NAME})
    target_include_directories(pull_example PRIVATE ${PROJECT_SOURCE_DIR})
    set_target_properties(pull_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Register Feed and Attribute example
    set(REGISTER_EXAMPLE_SOURCE_FILES examples/register_feed_and_attribute/Application.cpp)

    add_executable(register_example ${REGISTER_EXAMPLE_SOURCE_FILES})
    target_link_libraries(register_example ${PROJECT_NAME})
    target_include_directories(register_example PRIVATE ${PROJECT_SOURCE_DIR})
    set_target_properties(register_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Simple example
    set(SIMPLE_EXAMPLE_SOURCE_FILES examples/simple/Application.cpp)

    add_executable(simple_example ${SIMPLE_EXAMPLE_SOURCE_FILES})
    target_link_libraries(simple_example ${PROJECT_NAME})
    target_include_directories(simple_example PRIVATE ${PROJECT_SOURCE_DIR})
    set_target_properties(simple_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Subdevices example
    set(SUBDEVICES_EXAMPLE_SOURCE_FILES examples/subdevices/Application.cpp)

    add_executable(subdevices_example ${SUBDEVICES_EXAMPLE_SOURCE_FILES})
    target_link_libraries(subdevices_example ${PROJECT_NAME})
    target_include_directories(subdevices_example PRIVATE ${PROJECT_SOURCE_DIR})
    set_target_properties(subdevices_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

    # Debian package installation example
    if (${BUILD_APT_SYSTEMD_FIRMWARE_UPDATER})
        set(DEBIAN_EXAMPLE_SOURCE_FILES examples/debian-package-installation/Application.cpp)

        add_executable(debian_package_example ${DEBIAN_EXAMPLE_SOURCE_FILES})
        target_link_libraries(debian_package_example ${PROJECT_NAME})
        target_include_directories(debian_package_example PRIVATE ${PROJECT_SOURCE_DIR})
        set_target_properties(debian_package_example PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    endif ()
endif ()

# Make the install permissions rule
if (${BUILD_APT_SYSTEMD_FIRMWARE_UPDATER})
    add_custom_target(install-permissions)
    add_custom_command(TARGET install-permissions COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/out/manage-units.pkla /etc/polkit-1/localauthority/50-local.d)
endif ()

# Create the install rule
include(GNUInstallDirs)
install(DIRECTORY ${CMAKE_LIBRARY_INCLUDE_DIRECTORY} DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN *.h)
install(DIRECTORY ${CMAKE_BINARY_DIR}/include DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN *.h)
install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

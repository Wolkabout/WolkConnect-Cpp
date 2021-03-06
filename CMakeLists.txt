# Copyright 2020 WolkAbout Technology s.r.o.
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
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}    -Wall -Wextra -pedantic                                                    \
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
                                            -Wwrite-strings                                                            \
                                            -fprofile-arcs -ftest-coverage -g")

# library output
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/build)
set(CMAKE_LIBRARY_INCLUDE_DIRECTORY ${CMAKE_PREFIX_PATH}/include)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
link_directories(${CMAKE_PREFIX_PATH}/lib)

find_package(Threads REQUIRED)

# WolkAbout c++ SDK
option(POCO_BUILD_DATA "" ON)
option(POCO_BUILD_NET "" ON)
add_subdirectory(WolkSDK-Cpp)

# WolkAbout c++ Connector
file(GLOB_RECURSE LIB_HEADER_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.h" "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp")
file(GLOB_RECURSE LIB_SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.c" "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCE_FILES} ${LIB_HEADER_FILES})
target_link_libraries(${PROJECT_NAME} WolkAboutCore z PocoUtil PocoCrypto PocoData PocoDataSQLite PocoFoundation Threads::Threads)
target_include_directories(${PROJECT_NAME} PUBLIC "src")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_LIBRARY_INCLUDE_DIRECTORY})
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")
add_dependencies(${PROJECT_NAME} libpoco)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")

# Tests
include_directories("tests")

file(GLOB_RECURSE TEST_HEADER_FILES "tests/*.h" "tests/*.hpp")
file(GLOB_RECURSE TEST_SOURCE_FILES "tests/*.c" "tests/*.cpp")

add_executable(${PROJECT_NAME}Tests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})
target_link_libraries(${PROJECT_NAME}Tests ${PROJECT_NAME} gtest_main gtest gmock_main gmock)
target_include_directories(${PROJECT_NAME}Tests SYSTEM PUBLIC ${CMAKE_LIBRARY_INCLUDE_DIRECTORY})
set_target_properties(${PROJECT_NAME}Tests PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
set_target_properties(${PROJECT_NAME}Tests PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_dependencies(${PROJECT_NAME}Tests ${PROJECT_NAME} libgtest)

add_custom_target(tests ${PROJECT_NAME}Tests)
add_test(gtest ${PROJECT_NAME}Tests)

# Examples

# Full example
file(GLOB_RECURSE FULL_EXAMPLE_HEADER_FILES "examples/full_feature_set/*.h")
file(GLOB_RECURSE FULL_EXAMPLE_SOURCE_FILES "examples/full_feature_set/*.cpp")

add_executable(full_example ${FULL_EXAMPLE_SOURCE_FILES})
target_link_libraries(full_example ${PROJECT_NAME})
set_target_properties(full_example PROPERTIES INSTALL_RPATH "$ORIGIN/lib")

# Simple example
file(GLOB_RECURSE SIMPLE_EXAMPLE_HEADER_FILES "examples/simple/*.h")
file(GLOB_RECURSE SIMPLE_EXAMPLE_SOURCE_FILES "examples/simple/*.cpp")

add_executable(simple_example ${SIMPLE_EXAMPLE_SOURCE_FILES})
target_link_libraries(simple_example ${PROJECT_NAME})
set_target_properties(simple_example PROPERTIES INSTALL_RPATH "$ORIGIN/lib")

add_subdirectory(cmake)

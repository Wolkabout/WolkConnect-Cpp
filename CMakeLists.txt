# Copyright 2018 WolkAbout Technology s.r.o.
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

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}    -Wall -Wextra -pedantic                                                    \
                                            -pedantic-errors -Wcast-align                                              \
                                            -Wcast-qual -Wconversion                                                   \
                                            -Wdisabled-optimization                                                    \
                                            -Wfloat-equal -Wformat=2                                                   \
                                            -Wformat-nonliteral -Wformat-security                                      \
                                            -Wformat-y2k                                                               \
                                            -Wimport  -Winit-self                                                      \
                                            -Wmissing-field-initializers -Wmissing-format-attribute                    \
                                            -Wmissing-include-dirs -Wmissing-noreturn                                  \
                                            -Wpointer-arith                                                            \
                                            -Wno-packed  -Wno-padded -Wredundant-decls                                 \
                                            -Wshadow -Wstack-protector                                                 \
                                            -Wstrict-aliasing=2                                                        \
                                            -Wunreachable-code -Wno-unused                                             \
                                            -Wvariadic-macros                                                          \
                                            -Wwrite-strings")

# Paho MQTT
IF (NOT SSL_INCLUDE_DIR)
    message(FATAL_ERROR
            "SSL include dir must be specified with: -DSSL_INCLUDE_DIR=...")
ENDIF (NOT SSL_INCLUDE_DIR)

IF (NOT IS_ABSOLUTE ${SSL_INCLUDE_DIR})
    set(SSL_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/${SSL_INCLUDE_DIR}")
ENDIF (NOT IS_ABSOLUTE ${SSL_INCLUDE_DIR})

set(SSL_LIBS ssl crypto)

add_definitions(-DOPENSSL)

# library output
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
link_directories(${CMAKE_BINARY_DIR}/lib)

# WolkAbout c++ SDK
add_subdirectory(WolkSDK-Cpp)

# WolkAbout c++ Connector
include_directories("src")
include_directories("WolkSDK-Cpp/mqtt")
include_directories("WolkSDK-Cpp/core")

file(GLOB_RECURSE LIB_HEADER_FILES "src/*.h" "src/*.hpp")
file(GLOB_RECURSE LIB_SOURCE_FILES "src/*.cpp")

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} WolkAboutCore)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-rpath,./")

# Examples

# Full example
file(GLOB_RECURSE FULL_EXAMPLE_HEADER_FILES "examples/full_feature_set/*.h")
file(GLOB_RECURSE FULL_EXAMPLE_SOURCE_FILES "examples/full_feature_set/*.cpp")

add_executable(full_example ${FULL_EXAMPLE_SOURCE_FILES})
target_link_libraries(full_example ${PROJECT_NAME})
set_target_properties(full_example PROPERTIES LINK_FLAGS "-Wl,-rpath,./lib")

# Simple example
file(GLOB_RECURSE SIMPLE_EXAMPLE_HEADER_FILES "examples/simple/*.h")
file(GLOB_RECURSE SIMPLE_EXAMPLE_SOURCE_FILES "examples/simple/*.cpp")

add_executable(simple_example ${SIMPLE_EXAMPLE_SOURCE_FILES})
target_link_libraries(simple_example ${PROJECT_NAME})
set_target_properties(simple_example PROPERTIES LINK_FLAGS "-Wl,-rpath,./lib")


add_subdirectory(cmake)

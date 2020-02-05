/*
 * Copyright 2018 WolkAbout Technology s.r.o.
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
#ifndef FIRMWAREINSTALLER_H
#define FIRMWAREINSTALLER_H

#include <functional>
#include <string>

namespace wolkabout
{
class FirmwareInstaller
{
public:
    virtual ~FirmwareInstaller() = default;

    /**
     * @brief Install the firmware from provided file
     *
     * This call needs to return as quickly as possible
     *
     * @param firmwareFile Firmware file to install
     * @param onSuccess Function to call if install is successful
     * @param onFail Function to call if install has failed
     */
    virtual void install(const std::string& firmwareFile, std::function<void()> onSuccess,
                         std::function<void()> onFail) = 0;

    /**
     * @brief Abort firmware installation if possible
     *
     * This call needs to return as quickly as possible
     *
     * @return true if success or false if abort cannot be performed
     */
    virtual bool abort() = 0;
};
}    // namespace wolkabout

#endif    // FIRMWAREINSTALLER_H
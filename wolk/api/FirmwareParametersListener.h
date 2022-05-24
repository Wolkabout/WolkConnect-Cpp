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

#ifndef WOLKABOUTCONNECTOR_FIRMWAREPARAMETERSLISTENER_H
#define WOLKABOUTCONNECTOR_FIRMWAREPARAMETERSLISTENER_H

#include <string>

namespace wolkabout
{
namespace connect
{
/**
 * This is an interface that is meant for configurations where the device is supposed to only pull parameter information
 * about Firmware Update. This means the time when the device is supposed to check the repository for new updates.
 */
class FirmwareParametersListener
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~FirmwareParametersListener() = default;

    /**
     * This is a method that will be invoked with the parameters information when it is available.
     *
     * @param repository The link to the repository where the device should check for updates.
     * @param updateTime The time when the device should check for updates.
     */
    virtual void receiveParameters(std::string repository, std::string updateTime) = 0;

    /**
     * This is the method with which the service can ask for the current firmware version.
     *
     * @return The current firmware version.
     */
    virtual std::string getFirmwareVersion() = 0;
};
}    // namespace connect
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FIRMWAREPARAMETERSLISTENER_H

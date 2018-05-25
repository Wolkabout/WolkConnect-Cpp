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

#ifndef DEVICE_H
#define DEVICE_H

#include "model/DetailedDevice.h"

#include <map>
#include <string>
#include <vector>

namespace wolkabout
{
class Device : public DetailedDevice
{
public:
    /**
     * @brief Constructor
     * @param key Device key provided by WolkAbout IoT Platform
     * @param password Device password provided by WolkAbout IoT Platform
     * @param actuatorReferences List of actuator references
     */
    Device(std::string key, std::string password, std::vector<std::string> actuatorReferences = {});

    /**
     * @brief Add sensor to device
     * @param reference Sensor reference
     * @param delimiter Sensor delimiter
     */
    void addSensor(const std::string& reference, unsigned size = 1);

    /**
     * @brief Add configuration to device
     * @param reference Configuration reference
     * @param delimiter Configuration delimiter
     */
    void addConfiguration(const std::string& reference, unsigned size = 1);

    /**
     * @brief Returns actuator references for device
     * @return actuator references for device
     */
    std::vector<std::string> getActuatorReferences() const;

    /**
     * @brief Returns sensor delimiters
     * @return std::map with sensor reference as key, and sensor delimiter as value
     */
    std::map<std::string, std::string> getSensorDelimiters() const;

    /**
     * @brief Returns configuration delimiters
     * @return std::map with configuration reference as key, and configuration delimiter as value
     */
    std::map<std::string, std::string> getConfigurationDelimiters() const;
};
}    // namespace wolkabout

#endif    // DEVICE_H

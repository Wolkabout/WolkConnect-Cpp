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

#include "model/BasicDevice.h"

namespace wolkabout
{
BasicDevice::BasicDevice(std::string key, std::string password, std::vector<std::string> actuatorReferences)
: Device{"", key, password, DeviceManifest{"", "", "", ""}}
{
    for (const auto& reference : actuatorReferences)
    {
        m_deviceManifest.addActuator(
          ActuatorManifest{"", reference, "", "", "", ActuatorManifest::DataType::STRING, 1, 0, 1});
    }
}

void BasicDevice::addSensor(const std::string& reference, unsigned size, const std::string& delimiter)
{
    m_deviceManifest.addSensor(SensorManifest{"", reference, "", "", "", SensorManifest::DataType::STRING, 1, 0, 1,
                                              delimiter, std::vector<std::string>(size, "")});
}

void BasicDevice::addConfiguration(const std::string& reference, unsigned size, const std::string& delimiter)
{
    m_deviceManifest.addConfiguration(ConfigurationManifest{"", reference, "", "",
                                                            ConfigurationManifest::DataType::STRING, 0, 1, "", size,
                                                            delimiter, std::vector<std::string>(size, "")});
}

std::vector<std::string> BasicDevice::getActuatorReferences() const
{
    return Device::getActuatorReferences();
}

std::map<std::string, std::string> BasicDevice::getSensorDelimiters() const
{
    return Device::getSensorDelimiters();
}

std::map<std::string, std::string> BasicDevice::getConfigurationDelimiters() const
{
    return Device::getConfigurationDelimiters();
}
}    // namespace wolkabout

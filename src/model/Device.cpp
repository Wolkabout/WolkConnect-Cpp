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

#include "model/Device.h"

namespace wolkabout
{
Device::Device(std::string key, std::string password, std::vector<std::string> actuatorReferences)
: DetailedDevice{"", key, password, DeviceManifest{"", "", "", ""}}
{
    for (const auto& reference : actuatorReferences)
    {
        m_deviceManifest.addActuator(ActuatorManifest{"", reference, DataType::STRING, ""});
    }
}

void Device::addSensor(const std::string& reference, unsigned size)
{
    m_deviceManifest.addSensor(
      SensorManifest{"", reference, "", "", DataType::STRING, 1, "", std::vector<std::string>(size, "")});
}

void Device::addConfiguration(const std::string& reference, unsigned size)
{
    m_deviceManifest.addConfiguration(
      ConfigurationManifest{"", reference, DataType::STRING, "", "", std::vector<std::string>(size, "")});
}

std::vector<std::string> Device::getActuatorReferences() const
{
    return DetailedDevice::getActuatorReferences();
}

std::map<std::string, std::string> Device::getSensorDelimiters() const
{
    return DetailedDevice::getSensorDelimiters();
}

std::map<std::string, std::string> Device::getConfigurationDelimiters() const
{
    return DetailedDevice::getConfigurationDelimiters();
}
}    // namespace wolkabout

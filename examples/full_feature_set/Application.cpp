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

#include "Wolk.h"
#include "service/FirmwareInstaller.h"
#include "utilities/ConsoleLogger.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

int main(int /* argc */, char** /* argv */)
{
    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::DEBUG);
    wolkabout::Logger::setInstance(std::move(logger));

    wolkabout::Device device("device_key", "some_password", {"SW", "SL"});

    static bool switchValue = false;
    static int sliderValue = 0;

    class CustomFirmwareInstaller : public wolkabout::FirmwareInstaller
    {
    public:
        bool install(const std::string& firmwareFile) override
        {
            // Mock install
            LOG(INFO) << "Updating firmware with file " << firmwareFile;

            // Optionally delete 'firmwareFile
            return true;
        }
    };
    auto installer = std::make_shared<CustomFirmwareInstaller>();

    class DeviceConfiguration : public wolkabout::ConfigurationProvider, public wolkabout::ConfigurationHandler
    {
    public:
        DeviceConfiguration()
        {
            m_configuration.push_back(wolkabout::ConfigurationItem({"0"}, "config_1"));
            m_configuration.push_back(wolkabout::ConfigurationItem({"false"}, "config_2"));
            m_configuration.push_back(wolkabout::ConfigurationItem({""}, "config_3"));
        }

        std::vector<wolkabout::ConfigurationItem> getConfiguration() override
        {
            std::lock_guard<decltype(m_ConfigurationMutex)> l(m_ConfigurationMutex);    // Must be thread safe
            return m_configuration;
        }

        void handleConfiguration(const std::vector<wolkabout::ConfigurationItem>& configuration) override
        {
            std::lock_guard<decltype(m_ConfigurationMutex)> l(m_ConfigurationMutex);    // Must be thread safe
            m_configuration = configuration;
        }

    private:
        std::mutex m_ConfigurationMutex;
        std::vector<wolkabout::ConfigurationItem> m_configuration;
    };
    auto deviceConfiguration = std::make_shared<DeviceConfiguration>();

    std::unique_ptr<wolkabout::Wolk> wolk =
      wolkabout::Wolk::newBuilder(device)
        .actuationHandler([&](const std::string& reference, const std::string& value) -> void {
            LOG(DEBUG) << "Actuation request received - Reference: " << reference << " value: " << value;

            if (reference == "SW")
            {
                switchValue = value == "true" ? true : false;
            }
            else if (reference == "SL")
            {
                try
                {
                    sliderValue = std::stoi(value);
                }
                catch (...)
                {
                    sliderValue = 0;
                }
            }
        })
        .actuatorStatusProvider([&](const std::string& reference) -> wolkabout::ActuatorStatus {
            if (reference == "SW")
            {
                return wolkabout::ActuatorStatus(switchValue ? "true" : "false",
                                                 wolkabout::ActuatorStatus::State::READY);
            }
            else if (reference == "SL")
            {
                return wolkabout::ActuatorStatus(std::to_string(sliderValue), wolkabout::ActuatorStatus::State::READY);
            }

            return wolkabout::ActuatorStatus("", wolkabout::ActuatorStatus::State::READY);
        })
        .configurationHandler(deviceConfiguration)
        .configurationProvider(deviceConfiguration)
        .withFirmwareUpdate("2.1.0", installer, ".", 100 * 1024 * 1024, 1024 * 1024)
        .build();

    wolk->connect();

    wolk->addAlarm("HH", "High Humidity");

    wolk->addSensorReading("P", 25.6);
    wolk->addSensorReading("T", 1024);
    wolk->addSensorReading("H", 52);

    wolk->addSensorReading("ACL", {1, 0, 0});

    wolk->publish();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}

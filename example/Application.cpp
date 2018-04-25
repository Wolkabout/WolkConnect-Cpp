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

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

int main(int /* argc */, char** /* argv */)
{
    wolkabout::SensorManifest temperatureSensor{"Temperature",
                                                "T",
                                                "Temperature sensor",
                                                "℃",
                                                "TEMPERATURE",
                                                wolkabout::SensorManifest::DataType::NUMERIC,
                                                1,
                                                -273.15,
                                                100000000};
    wolkabout::SensorManifest pressureSensor{
      "Pressure", "P", "Pressure sensor", "mb", "PRESSURE", wolkabout::SensorManifest::DataType::NUMERIC, 1, 0, 1100};
    wolkabout::SensorManifest humiditySensor{
      "Humidity", "H", "Humidity sensor", "%", "HUMIDITY", wolkabout::SensorManifest::DataType::NUMERIC, 1, 0, 100};

    wolkabout::SensorManifest accelerationSensor{"Acceleration",
                                                 "ACL",
                                                 "Acceleration sensor",
                                                 "m/s²",
                                                 "ACCELEROMETER",
                                                 wolkabout::SensorManifest::DataType::NUMERIC,
                                                 1,
                                                 0,
                                                 1000,
                                                 "_",
                                                 {"x", "y", "z"}};

    wolkabout::ActuatorManifest switchActuator{
      "Switch", "SW", "Switch actuator", "", "SW", wolkabout::ActuatorManifest::DataType::BOOLEAN, 1, 0, 1};
    wolkabout::ActuatorManifest sliderActuator{
      "Slider", "SL", "Slider actuator", "", "SL", wolkabout::ActuatorManifest::DataType::NUMERIC, 1, 0, 100};

    wolkabout::AlarmManifest highHumidityAlarm{"High Humidity", wolkabout::AlarmManifest::AlarmSeverity::ALERT, "MA",
                                               "High Humidity", ""};

    wolkabout::ConfigurationManifest configurationItem1{
      "Item1", "KEY_1", "", "", wolkabout::ConfigurationManifest::DataType::STRING, 0, 0, "value1"};

    wolkabout::ConfigurationManifest configurationItem2{
      "Item2", "KEY_2",        "", "", wolkabout::ConfigurationManifest::DataType::NUMERIC, 0, 100, "50", 3,
      ":",     {"x", "y", "z"}};

    wolkabout::DeviceManifest deviceManifest{"manifest_name",
                                             "",
                                             "JsonProtocol",
                                             "",
                                             {configurationItem1, configurationItem2},
                                             {temperatureSensor, pressureSensor, humiditySensor, accelerationSensor},
                                             {highHumidityAlarm},
                                             {switchActuator, sliderActuator}};

    wolkabout::Device device("device_key", "some_password", deviceManifest);

    static bool switchValue = false;
    static int sliderValue = 0;

    class CustomFirmwareInstaller : public wolkabout::FirmwareInstaller
    {
    public:
        bool install(const std::string& firmwareFile) override
        {
            // Mock install
            std::cout << "Updating firmware with file " << firmwareFile << std::endl;

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
            m_configuration.push_back(wolkabout::ConfigurationItem({"configValue1"}, "KEY_1"));
            m_configuration.push_back(wolkabout::ConfigurationItem({"configValue2"}, "KEY_2"));
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
            std::cout << "Actuation request received - Reference: " << reference << " value: " << value << std::endl;

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

    wolk->addAlarm("MA", "High Humidity");

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

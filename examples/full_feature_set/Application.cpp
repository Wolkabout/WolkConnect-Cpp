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
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

std::function<void(int)> sigintCall;

void sigintResponse(int signal)
{
    if (sigintCall != nullptr)
        sigintCall(signal);
}

int main(int /* argc */, char** /* argv */)
{
    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::TRACE);
    wolkabout::Logger::setInstance(std::move(logger));

    wolkabout::Device device("ADC", "ITZ70HZGYB", {"SW", "SL"});

    static bool switchValue = false;
    static int sliderValue = 0;

    static int deviceFirmwareVersion = 1;

    class FirmwareInstallerImpl : public wolkabout::FirmwareInstaller
    {
    public:
        void install(const std::string& firmwareFile, std::function<void()> onSuccess,
                     std::function<void()> onFail) override
        {
            // Mock install
            LOG(INFO) << "Updating firmware with file " << firmwareFile;

            // Determine installation outcome and report it
            if (true)
            {
                ++deviceFirmwareVersion;
                onSuccess();
            }
            else
            {
                onFail();
            }
        }

        bool abort() override
        {
            LOG(INFO) << "Abort device firmware installation";
            // true if successfully aborted or false if abort can not be performed
            return true;
        }
    };

    class FirmwareVersionProviderImpl : public wolkabout::FirmwareVersionProvider
    {
    public:
        std::string getFirmwareVersion() override { return std::to_string(deviceFirmwareVersion) + ".0.0"; }
    };

    auto installer = std::make_shared<FirmwareInstallerImpl>();
    auto provider = std::make_shared<FirmwareVersionProviderImpl>();

    class DeviceConfiguration : public wolkabout::ConfigurationProvider, public wolkabout::ConfigurationHandler
    {
    public:
        DeviceConfiguration()
        {
            m_configuration.push_back(wolkabout::ConfigurationItem({"10"}, "HB"));
            m_configuration.push_back(wolkabout::ConfigurationItem({"P,T,H,ACL"}, "EF"));
            m_configuration.push_back(wolkabout::ConfigurationItem({"INFO"}, "LL"));
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
                switchValue = value == "true";
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
        .withFileManagement(".", 1024 * 1024)
        .withFirmwareUpdate(installer, provider)
        .build();

    wolk->connect();

    const auto& publishSensorsAndAlarm = [&]() {
        // Analyze the EF configuration to send only specific readings to the platform
        // if "P" is in "EF", send it
        // also for all the others
        // but if "H" is in "EF", send "H" and "HH"

        const auto& index =
          std::find_if(deviceConfiguration->getConfiguration().begin(), deviceConfiguration->getConfiguration().end(),
                       [&](const wolkabout::ConfigurationItem& config) { return config.getReference() == "EF"; });
        std::string configString;
        bool validConfig = false;

        if (index != deviceConfiguration->getConfiguration().end())
        {
            validConfig = true;
            configString = (*index).getValues()[0];
        }

        if (!validConfig || configString.find('P') != std::string::npos)
        {
            wolk->addSensorReading("P", (rand() % 1000 * 0.1));
        }

        if (!validConfig || configString.find('H') != std::string::npos)
        {
            const auto& val = (rand() % 1000 * 0.1);
            wolk->addSensorReading("H", val);
            if (val > 90)
            {
                wolk->addAlarm("HH", true);
            }
            else
            {
                wolk->addAlarm("HH", false);
            }
        }

        if (!validConfig || configString.find('T') != std::string::npos)
        {
            wolk->addSensorReading("T", (rand() % 1500));
        }

        if (!validConfig || configString.find("ACL") != std::string::npos)
        {
            wolk->addSensorReading("ACL", {rand() % 10000 * 0.001, rand() % 10000 * 0.001, rand() % 10000 * 0.001});
        }

        wolk->publish();
    };

    publishSensorsAndAlarm();

    uint16_t heartbeat = 0;
    for (const auto& config : deviceConfiguration->getConfiguration())
    {
        if (config.getReference() == "HB")
        {
            heartbeat = static_cast<uint16_t>(std::stoi(config.getValues()[0]));
        }
    }

    bool running = true;
    sigintCall = [&](int signal) {
        LOG(WARN) << "Application: Received stop signal, disconnecting...";
        running = false;
        if (wolk)
            wolk->disconnect();
        exit(signal);
    };
    signal(SIGINT, sigintResponse);

    while (running)
    {
        for (const auto& config : deviceConfiguration->getConfiguration())
        {
            if (config.getReference() == "HB")
            {
                heartbeat = static_cast<uint16_t>(std::stoi(config.getValues()[0]));
            }
        }
        LOG(DEBUG) << "Heartbeat: " << heartbeat << ", ";

        if (running)
            std::this_thread::sleep_for(std::chrono::seconds(heartbeat));

        publishSensorsAndAlarm();
    }

    wolk->disconnect();
    return 0;
}

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
#include "utilities/FileSystemUtils.h"
#include "utilities/json.hpp"

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

const std::string configJsonPath = "./configuration.json";

bool writeFile(const std::string& path, const std::vector<wolkabout::ConfigurationItem>& configuration)
{
    std::string content = "[";

    for (uint i = 0; i < configuration.size(); i++)
    {
        const auto& config = configuration.at(i);

        const auto& obj = nlohmann::json{{"reference", config.getReference()}, {"values", config.getValues()}};

        content += obj.dump();

        if (i < (configuration.size() - 1))
        {
            content += ',';
        }
    }

    content += "]";

    try
    {
        wolkabout::FileSystemUtils::createFileWithContent(path, content);
    }
    catch (std::exception& e)
    {
        LOG(ERROR) << e.what();
        throw std::logic_error("Unable to write configurations to output file.");
    }

    return true;
}

std::vector<wolkabout::ConfigurationItem> readFile(const std::string& path)
{
    if (!wolkabout::FileSystemUtils::isFilePresent(path))
    {
        //        throw std::logic_error("Given file does not exist (" + path + ").");
        return std::vector<wolkabout::ConfigurationItem>();
    }

    std::string jsonString;
    if (!wolkabout::FileSystemUtils::readFileContent(path, jsonString))
    {
        throw std::logic_error("Unable to read file (" + path + ").");
    }

    try
    {
        const auto& arr = nlohmann::json::parse(jsonString);
        std::vector<wolkabout::ConfigurationItem> configuration;

        for (const auto& obj : arr)
        {
            configuration.emplace_back(obj["values"], obj["reference"]);
        }

        return configuration;
    }
    catch (std::exception&)
    {
        throw std::logic_error("Unable to parse file (" + path + ").");
    }
}

int main(int /* argc */, char** /* argv */)
{
    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::INFO);
    wolkabout::Logger::setInstance(std::move(logger));

    std::function<void(std::string)> setLogLevel = [&](std::string level) {
        std::transform(level.begin(), level.end(), level.begin(), [&](char c) { return std::tolower(c); });

        if (level == "trace")
        {
            wolkabout::Logger::getInstance()->setLogLevel(wolkabout::LogLevel::TRACE);
        }
        else if (level == "debug")
        {
            wolkabout::Logger::getInstance()->setLogLevel(wolkabout::LogLevel::DEBUG);
        }
        else if (level == "info")
        {
            wolkabout::Logger::getInstance()->setLogLevel(wolkabout::LogLevel::INFO);
        }
        else if (level == "warn")
        {
            wolkabout::Logger::getInstance()->setLogLevel(wolkabout::LogLevel::WARN);
        }
        else if (level == "error")
        {
            wolkabout::Logger::getInstance()->setLogLevel(wolkabout::LogLevel::ERROR);
        }
    };

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
            const auto& value = readFile(configJsonPath);
            if (value.empty())
            {
                const auto& hb = wolkabout::ConfigurationItem({"10"}, "HB");
                const auto& ef = wolkabout::ConfigurationItem({"P,T,H,ACL"}, "EF");
                const auto& ll = wolkabout::ConfigurationItem({"INFO"}, "LL");

                m_configuration.emplace("HB", hb);
                m_configuration.emplace("EF", ef);
                m_configuration.emplace("LL", ll);

                writeFile(configJsonPath, {hb, ef, ll});
            }
            else
            {
                for (const auto& config : value)
                {
                    m_configuration.emplace(config.getReference(), config);
                }
            }
        }

        std::vector<wolkabout::ConfigurationItem> getConfiguration() override
        {
            std::lock_guard<decltype(m_ConfigurationMutex)> l(m_ConfigurationMutex);    // Must be thread safe
            std::vector<wolkabout::ConfigurationItem> configurations;
            for (const auto& config : m_configuration)
            {
                configurations.emplace_back(config.second);
            }
            return configurations;
        }

        void handleConfiguration(const std::vector<wolkabout::ConfigurationItem>& configuration) override
        {
            std::lock_guard<decltype(m_ConfigurationMutex)> l(m_ConfigurationMutex);    // Must be thread safe
            for (const auto& config : configuration)
            {
                const auto& it = m_configuration.find(config.getReference());
                if (it != m_configuration.end())
                {
                    it->second = config;
                }
                else
                {
                    m_configuration.emplace(config.getReference(), config);
                }
            }

            std::vector<wolkabout::ConfigurationItem> configVector;
            for (const auto& config : m_configuration)
            {
                configVector.emplace_back(config.second);
            }

            writeFile(configJsonPath, configVector);
        }

    private:
        std::mutex m_ConfigurationMutex;
        std::map<std::string, wolkabout::ConfigurationItem> m_configuration;
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

    uint16_t heartbeat = 0;
    for (const auto& config : deviceConfiguration->getConfiguration())
    {
        if (config.getReference() == "HB")
        {
            heartbeat = static_cast<uint16_t>(std::stoi(config.getValues()[0]));
        }
        if (config.getReference() == "LL")
        {
            if (setLogLevel)
                setLogLevel(config.getValues()[0]);
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
            if (config.getReference() == "LL")
            {
                if (setLogLevel)
                    setLogLevel(config.getValues()[0]);
            }
        }

        LOG(DEBUG) << "Heartbeat is: " << heartbeat;
        LOG(DEBUG) << "Last received timestamp : " << wolk->getLastTimestamp();

        if (running)
            std::this_thread::sleep_for(std::chrono::seconds(heartbeat));

        const auto& configVector = deviceConfiguration->getConfiguration();

        auto index =
          std::find_if(configVector.begin(), configVector.end(),
                       [&](const wolkabout::ConfigurationItem& config) { return config.getReference() == "EF"; });
        std::string configString;
        bool validConfig = false;

        if (index != configVector.end())
        {
            validConfig = true;
            if (!index->getValues().empty())
                configString = index->getValues()[0];
        }

        if (!validConfig || configString.find('P') != std::string::npos)
        {
            wolk->addSensorReading("P", (rand() % 801 + 300));
        }

        if (!validConfig || configString.find('H') != std::string::npos)
        {
            const auto& val = (rand() % 1000 * 0.1);
            wolk->addSensorReading("H", val);
            wolk->addAlarm("HH", val > 90);
        }

        if (!validConfig || configString.find('T') != std::string::npos)
        {
            wolk->addSensorReading("T", (rand() % 126 - 40));
        }

        if (!validConfig || configString.find("ACL") != std::string::npos)
        {
            wolk->addSensorReading("ACL", {rand() % 100001 * 0.001, rand() % 100001 * 0.001, rand() % 100001 * 0.001});
        }

        wolk->publish();
    }

    wolk->disconnect();
    return 0;
}

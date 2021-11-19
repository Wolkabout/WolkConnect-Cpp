/**
 * Copyright 2021 WolkAbout Technology s.r.o.
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

#include "core/persistence/inmemory/InMemoryPersistence.h"
#include "core/service/FirmwareInstaller.h"
#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"
#include "core/utilities/json.hpp"
#include "wolk/Wolk.h"

#include <chrono>
#include <csignal>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>

/**
 * This is the place where user input is required for running the example.
 * In here, you can enter the device credentials to successfully identify the device on the platform.
 * And also, the target platform path, and the SSL certificate that is used to establish a secure connection.
 */
const std::string DEVICE_KEY = "ACFE";
const std::string DEVICE_PASSWORD = "IO62M61QOR";
const std::string PLATFORM_HOST = "ssl://integration5.wolkabout.com:8883";
const std::string CA_CERT_PATH = "./ca.crt";
const std::string FILE_MANAGEMENT_LOCATION = "./files";

/**
 * This is a structure definition that is a collection of all information/feeds the device will have.
 */
struct DeviceData
{
    std::double_t temperature;
    bool toggle;
    std::chrono::seconds heartbeat;
    std::string firmwareVersion;
};

/**
 * This class represents an object capable of storing feed values into the persistence.
 * This can be used to retrieve old feed values after running the program to keep the device state consistent.
 */
class FeedPersistence
{
public:
    /**
     * This is the default constructor for the feeds that will create the map for all feeds, in which values can be
     * loaded/saved.
     *
     * @param feedNames The list of all feeds that will be in this session.
     */
    explicit FeedPersistence(const std::vector<std::string>& feedNames)
    {
        for (const auto& feedName : feedNames)
            m_feeds.emplace(feedName, std::string{});
    }

private:
    // This is the constant value representing the location of the JSON file used to load/store values.
    static const std::string JSON_FILE_LOCATION;

    // This is the place where in memory the values are going to be stored.
    std::map<std::string, std::string> m_feeds;
};

/**
 * This is an object that is capable of handling the received data about feeds from the platform.
 * It will interact with the DeviceData object in which it store the information. It can also notify the persistence, so
 * the cold-storage information can be updated too.
 */
class DeviceDataChangeHandler : public wolkabout::FeedUpdateHandler
{
public:
    /**
     * Default constructor that will establish the relationship between the handler and the data.
     *
     * @param deviceData The data object in which the handler will put the data.
     * @param persistence The optional persistence object that will be notified when the data is changed.
     */
    explicit DeviceDataChangeHandler(DeviceData& deviceData, std::shared_ptr<FeedPersistence> persistence = nullptr)
    : m_deviceData(deviceData), m_persistence(std::move(persistence))
    {
    }

    /**
     * Default setter for the persistence that will be notified by the change handler about device information.
     *
     * @param persistence Pointer to the new persistence that will be used by the handler.
     */
    void setPersistence(std::shared_ptr<FeedPersistence> persistence) { m_persistence = std::move(persistence); }

    /**
     * This is the overridden method from the `FeedUpdateHandler` interface.
     * This is the method that will receive information about a feed.
     *
     * @param map The map containing information about updated feeds and their new value(s).
     */
    void handleUpdate(std::map<std::uint64_t, std::vector<wolkabout::Reading>> map) override
    {
        // TODO
    }

private:
    // This is where the object containing all information about the device is stored, and the optional pointer to
    // persistence.
    DeviceData& m_deviceData;
    std::shared_ptr<FeedPersistence> m_persistence;
};

const std::string FeedPersistence::JSON_FILE_LOCATION = "./feeds.json";

/**
 * This is interrupt logic used to stop the application from running.
 */
std::function<void(int)> sigintCall;

void sigintResponse(int signal)
{
    if (sigintCall != nullptr)
        sigintCall(signal);
}

int main(int /* argc */, char** /* argv */)
{
    /**
     * Setting up the default console logger.
     * For more info, increase the LogLevel.
     * Logging to file could also be added here, by adding the type `Logger::Type::FILE` and passing a file path as
     * third argument.
     */
    wolkabout::Logger::init(wolkabout::LogLevel::TRACE, wolkabout::Logger::Type::CONSOLE);

    /**
     * Now we can create the device using the user provided device credentials.
     * We will also create the data object, in which we store the state of all values of the device.
     * And the persistence object, that will be storing the values in cold-storage, that would be available to us at any
     * moment. There are only two values that the platform could send us - Switch ("SW"), or Heartbeat ("HB"), so only
     * those two will be stored by the persistence.
     */
    auto device = wolkabout::Device{DEVICE_KEY, DEVICE_PASSWORD, wolkabout::OutboundDataMode::PUSH};
    auto deviceInfo = DeviceData{0, false, std::chrono::seconds(3), "1.0.0-DEVELOPMENT"};
    auto infoPersistence = std::make_shared<FeedPersistence>(std::vector<std::string>{"SW", "HB"});
    auto deviceInfoHandler = std::make_shared<DeviceDataChangeHandler>(deviceInfo, infoPersistence);

    /**
     * Now we can start creating the Wolk instance that is right for us.
     * We will also create some in memory persistence so messages can get buffered if the platform connection gets
     * interrupted.
     */
    auto inMemoryPersistence = std::make_shared<wolkabout::InMemoryPersistence>();
    auto wolk = wolkabout::WolkBuilder(device)
                  .host(PLATFORM_HOST)
                  .ca_cert_path(CA_CERT_PATH)
                  .feedUpdateHandler(deviceInfoHandler)
                  .withPersistence(inMemoryPersistence)
                  .withFileManagement(FILE_MANAGEMENT_LOCATION)
                  .build();

    /**
     * Now we can start the running logic of the connector. We will connect to the platform, and start running a loop
     * that will update the temperature feed. This loop will also be affected by the heartbeat feed.
     */
    wolk->connect();
    bool running = true;
    sigintCall = [&](int signal)
    {
        LOG(WARN) << "Application: Received stop signal, disconnecting...";
        running = false;
        if (wolk)
            wolk->disconnect();
        exit(signal);
    };
    signal(SIGINT, sigintResponse);

    /**
     * We want to randomize the temperature data too, so we need the generator for random information.
     */
    auto engine = std::mt19937{static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    auto distribution = std::uniform_real_distribution<std::double_t>{-20.0, 80.0};

    while (running)
    {
        // Generate the new value
        deviceInfo.temperature = distribution(engine);

        // Publish the information
        wolk->addReading("T", deviceInfo.temperature);
        wolk->addReading("SW", deviceInfo.toggle);
        wolk->addReading("HB", deviceInfo.heartbeat.count());
        wolk->publish();

        // And sleep in the loop
        if (running)
            std::this_thread::sleep_for(std::chrono::seconds(deviceInfo.heartbeat));
    }

    wolk->disconnect();
    return 0;
}

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
#include "core/utilities/FileSystemUtils.h"
#include "core/utilities/Logger.h"
#include "core/utilities/json.hpp"
#include "wolk/Wolk.h"

#include <chrono>
#include <csignal>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>

/**
 * This is the place where user input is required for running the example.
 * In here, you can enter the device credentials to successfully identify the device on the platform.
 * And also, the target platform path, and the SSL certificate that is used to establish a secure connection.
 */
const std::string DEVICE_KEY = "AWC";
const std::string DEVICE_PASSWORD = "VZ8R3MI87R";
const std::string PLATFORM_HOST = "ssl://integration5.wolkabout.com:8883";
const std::string CA_CERT_PATH = "./ca.crt";
const std::string FILE_MANAGEMENT_LOCATION = "./files";
const std::string FIRMWARE_VERSION = "4.0.0";

/**
 * This is a structure definition that is a collection of all information/feeds the device will have.
 */
struct DeviceData
{
    std::double_t temperature;
    bool toggle;
    std::chrono::seconds heartbeat;
};

// Here we have some synchronization tools
std::mutex mutex;
std::condition_variable conditionVariable;

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
     */
    explicit DeviceDataChangeHandler(DeviceData& deviceData) : m_deviceData(deviceData) {}

    /**
     * This is the overridden method from the `FeedUpdateHandler` interface.
     * This is the method that will receive information about a feed.
     *
     * @param readings The map containing information about updated feeds and their new value(s).
     */
    void handleUpdate(const std::string& deviceKey,
                      const std::map<std::uint64_t, std::vector<wolkabout::Reading>>& readings) override
    {
        // Go through all the timestamps
        for (const auto& pair : readings)
        {
            LOG(DEBUG) << "Received feed information for time: " << pair.first;

            // Take the readings, and apply them
            for (const auto& reading : pair.second)
            {
                LOG(DEBUG) << "Received feed information for reference '" << reading.getReference() << "'.";

                // Lock the mutex
                std::lock_guard<std::mutex> lock{mutex};

                // Check the reference on the readings
                if (reading.getReference() == "SW")
                    m_deviceData.toggle = reading.getBoolValue();
                else if (reading.getReference() == "HB")
                    m_deviceData.heartbeat = std::chrono::seconds(reading.getUIntValue());
            }

            // Notify the condition variable
            conditionVariable.notify_one();
        }
    }

private:
    // This is where the object containing all information about the device is stored.
    DeviceData& m_deviceData;
};

/**
 * This is an example implementation of the `FirmwareInstaller` interface. This class will ask the user about
 * preferences for return values of the methods.
 */
class ExampleFirmwareInstaller : public wolkabout::FirmwareInstaller
{
public:
    /**
     * Default constructor for the FirmwareInstaller. Requires the folder location for FileManagement files.
     *
     * @param fileLocation The location where files are located.
     */
    explicit ExampleFirmwareInstaller(std::string fileLocation) : m_fileLocation(std::move(fileLocation)) {}

    wolkabout::InstallResponse installFirmware(const std::string& deviceKey, const std::string& fileName) override
    {
        // Compose the path
        auto path = wolkabout::FileSystemUtils::composePath(fileName, m_fileLocation);
        LOG(INFO) << "Installation for file '" << path << "' on device '" << deviceKey << "' requested.";
        return wolkabout::InstallResponse::WILL_INSTALL;
    }

    void abortFirmwareInstall(const std::string& deviceKey) override
    {
        LOG(INFO) << "The firmware install on device '" << deviceKey << "' was aborted!";
    }

    std::string getFirmwareVersion(const std::string&) override { return FIRMWARE_VERSION; }

private:
    // Here is the folder location where files managed with FileManagement are stored.
    std::string m_fileLocation;
};

/**
 * This is an example implementation of the `FirmwareParametersListener` interface. This class will log the parameters
 * once they have been received from the platform.
 */
class ExampleFirmwareParameterListener : public wolkabout::FirmwareParametersListener
{
public:
    /**
     * This is an overridden method from the `FirmwareParameterListener` interface.
     * This method will be invoked once the service has received parameters regarding the firmware update.
     *
     * @param repository The repository link that should be checked.
     * @param updateTime The time at which the link should be checked.
     */
    void receiveParameters(std::string repository, std::string updateTime) override
    {
        LOG(INFO) << "Firmware Update Repository: " << repository;
        LOG(INFO) << "Firmware Update Time: " << updateTime;
    }

    std::string getFirmwareVersion() override { return FIRMWARE_VERSION; }
};

/**
 * This is an example implementation of the `FileListener` interface. This class will log when a file gets
 * added/removed.
 */
class ExampleFileListener : public wolkabout::FileListener
{
public:
    /**
     * This is an overridden method from the `FileListener` interface. This is a method that will be invoked once a file
     * has been added.
     *
     * @param deviceKey The device key for the file that has been added.
     * @param fileName The name of the file that has been added.
     * @param absolutePath The absolute path to the file that has been added.
     */
    void onAddedFile(const std::string& deviceKey, const std::string& fileName,
                     const std::string& absolutePath) override
    {
        LOG(INFO) << "A file has been added! -> '" << fileName << "' | '" << absolutePath << "' (on device '"
                  << deviceKey << "').";
    }

    /**
     * This is an overridden method from the `FileListener` interface. This is a method that will be invoked once a file
     * has been removed.
     *
     * @param deviceKey The device key for the file that has been removed.
     * @param fileName The name of the file that has been removed.
     */
    void onRemovedFile(const std::string& deviceKey, const std::string& fileName) override
    {
        LOG(INFO) << "A file has been removed! -> '" << fileName << "' (on device '" << deviceKey << "').";
    }
};

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
    auto deviceInfo = DeviceData{0, false, std::chrono::seconds(60)};
    auto deviceInfoHandler = std::make_shared<DeviceDataChangeHandler>(deviceInfo);

    /**
     * Now we can start creating the Wolk instance that is right for us.
     * We will also create some in memory persistence so messages can get buffered if the platform connection gets
     * interrupted.
     */
    auto inMemoryPersistence = std::make_shared<wolkabout::InMemoryPersistence>();
    auto wolk = wolkabout::WolkBuilder(device)
                  .host(PLATFORM_HOST)
                  .caCertPath(CA_CERT_PATH)
                  .feedUpdateHandler(deviceInfoHandler)
                  .withPersistence(inMemoryPersistence)
                  .withFileTransfer(FILE_MANAGEMENT_LOCATION)
                  // Uncomment for FileURLDownload
                  //                  .withFileURLDownload(FILE_MANAGEMENT_LOCATION, nullptr, true)
                  // Uncomment for a FileListener
                  .withFileListener(std::make_shared<ExampleFileListener>())
                  .withFirmwareUpdate(
                    std::unique_ptr<ExampleFirmwareInstaller>(new ExampleFirmwareInstaller(FILE_MANAGEMENT_LOCATION)))
                  // Uncomment for example ParameterListener
                  //                  .withFirmwareUpdate(std::make_shared<ExampleFirmwareParameterListener>())
                  .build();

    /**
     * Now we can start the running logic of the connector. We will connect to the platform, and start running a loop
     * that will update the temperature feed. This loop will also be affected by the heartbeat feed.
     */
    wolk->connect();
    bool running = true;
    sigintCall = [&](int) {
        LOG(WARN) << "Application: Received stop signal, disconnecting...";
        conditionVariable.notify_one();
        running = false;
    };
    signal(SIGINT, sigintResponse);

    /**
     * We want to randomize the temperature data too, so we need the generator for random information.
     */
    auto engine = std::mt19937{static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
    auto distribution = std::uniform_real_distribution<std::double_t>{-20.0, 80.0};

    while (running)
    {
        // Make place for the sleep interval
        auto sleepInterval = std::chrono::seconds{};

        {
            std::lock_guard<std::mutex> lock{mutex};

            // Check if the heartbeat is 0
            if (deviceInfo.heartbeat.count() == 0)
                continue;

            // Generate the new value
            deviceInfo.temperature = distribution(engine);

            // Publish the information
            wolk->addReading("T", deviceInfo.temperature);
            wolk->addReading("SW", deviceInfo.toggle);
            wolk->addReading("HB", deviceInfo.heartbeat.count());
            wolk->publish();

            // Obtain the old value
            sleepInterval = deviceInfo.heartbeat;
        }

        // And sleep in the loop
        if (running)
        {
            auto lock = std::unique_lock<std::mutex>{mutex};
            conditionVariable.wait_for(lock, sleepInterval,
                                       [&]() { return sleepInterval.count() != deviceInfo.heartbeat.count(); });
        }
    }

    wolk->disconnect();
    return 0;
}

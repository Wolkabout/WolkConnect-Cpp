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

#include "core/utilities/Logger.h"
#include "wolk/WolkBuilder.h"
#include "wolk/WolkSingle.h"
#include "wolk/api/ParameterHandler.h"

/**
 * This is the place where user input is required for running the example.
 * In here, you can enter the device credentials to successfully identify the device on the platform.
 * And also, the target platform path, and the SSL certificate that is used to establish a secure connection.
 */
const std::string DEVICE_KEY = "<DEVICE_KEY>";
const std::string DEVICE_PASSWORD = "<DEVICE_PASSWORD>";
const std::string PLATFORM_HOST = "ssl://demo.wolkabout.com:8883";
const std::string CA_CERT_PATH = "./ca.crt";

/**
 * This is a structure definition that is a collection of all information/feeds the device will have.
 */
struct DeviceData
{
    bool toggle;
    std::chrono::seconds heartbeat;
};

/**
 * This is an implementation of a class that is able to receive new feed values from the platform.
 * When the device is going to pull feed values, this object is going to receive the values.
 */
class FeedChangeHandler : public wolkabout::connect::FeedUpdateHandler
{
public:
    /**
     * Default constructor with the reference to the object in which the values are going to be stored.
     *
     * @param deviceData The device data instance in which feed values are going to be written in.
     */
    explicit FeedChangeHandler(DeviceData& deviceData) : m_deviceData(deviceData) {}

    /**
     * This is the overridden method from the `wolkabout::FeedUpdateHandler` interface. This method will be invoked once
     * new feed values have been received.
     *
     * @param readings The map containing all the new feed values. Feed values are sent out grouped by their timestamp,
     * so the key in this map is going to be time at which values have been sent, and in the value is the vector of
     * values that have been set at that particular time.
     */
    void handleUpdate(const std::string& deviceKey,
                      const std::map<std::uint64_t, std::vector<wolkabout::Reading>>& readings) override
    {
        // Go through all the timestamps - since the `std::map` sorts by key, this will always go from the oldest to
        // newest.
        for (const auto& pair : readings)
        {
            // Take the readings, check if any of them interest us, and set the values in our object.
            for (const auto& reading : pair.second)
            {
                // Check the reference on the readings to check if we want to read values of them.
                // Reading values can always be extracted as `getStringValue()` (or `getStringValues()` for vectors).
                // If the value can be parsed into, for example, boolean, you can also do `getBoolValue()`.
                if (reading.getReference() == "SW")
                {
                    LOG(INFO) << "Received update for feed '" << reading.getReference() << "' - Value: '"
                              << reading.getStringValue() << "' | Time = " << pair.first << ".";
                    m_deviceData.toggle = reading.getBoolValue();
                }
                else if (reading.getReference() == "HB")
                {
                    LOG(INFO) << "Received update for feed '" << reading.getReference() << "' - Value: '"
                              << reading.getStringValue() << "' | Time = " << pair.first << ".";
                    m_deviceData.heartbeat = std::chrono::seconds(reading.getUIntValue());
                }
            }
        }
    }

private:
    // This is where we're going to store the reference to the object in which we're going to store feed values.
    DeviceData& m_deviceData;
};

/**
 * This is an implementation of a class that can receive parameter value updates, in the same way FeedChangeHandler can
 * receive feed updates. When the device decides to pull the values, this object will receive the values.
 */
class ParameterChangeHandler : public wolkabout::connect::ParameterHandler
{
public:
    /**
     * This is the overridden method from the `wolkabout::ParameterHandler` interface. This method will be invoked once
     * new parameter values have been received.
     *
     * @param parameters This is a vector containing all parameter that have been changed. Since the device pulls
     * parameters, the device will receive all updates to values since the last time it has pulled values.
     */
    void handleUpdate(const std::string& deviceKey, const std::vector<wolkabout::Parameter>& parameters) override
    {
        for (const auto& parameter : parameters)
            LOG(INFO) << "Received update for parameter '" << wolkabout::toString(parameter.first) << "' - Value: '"
                      << parameter.second << "'.";
    }
};

int main(int /* argc */, char** /* argv */)
{
    // This is the logger setup. Here you can set up the level of logging you would like enabled.
    wolkabout::Logger::init(wolkabout::LogLevel::INFO, wolkabout::Logger::Type::CONSOLE);

    // Here we create the device that we are presenting as on the platform.
    auto device = wolkabout::Device(DEVICE_KEY, DEVICE_PASSWORD, wolkabout::OutboundDataMode::PULL);
    auto data = DeviceData{};

    // And now we can create the handlers
    auto feedHandler = std::make_shared<FeedChangeHandler>(data);
    auto parameterHandler = std::make_shared<ParameterChangeHandler>();

    // And here we create the wolk session
    // Here we will set the feed value and parameter handler
    auto wolk = wolkabout::connect::WolkSingle::newBuilder(device)
                  .host(PLATFORM_HOST)
                  .caCertPath(CA_CERT_PATH)
                  .feedUpdateHandler(feedHandler)
                  .parameterHandler(parameterHandler)
                  .buildWolkSingle();

    // And now we will periodically connect, pull values, maybe even send some of our own, and then disconnect
    while (true)
    {
        // So we connect
        wolk->connect();

        // We request some values
        wolk->pullFeedValues();
        wolk->pullParameters();

        // Sleep a bit, and wait to send some of our own
        std::this_thread::sleep_for(std::chrono::seconds(2));
        wolk->addReading("SW", false);
        wolk->publish();

        // And then we disconnect
        std::this_thread::sleep_for(std::chrono::seconds(8));
        wolk->disconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    wolk->disconnect();
    return 0;
}

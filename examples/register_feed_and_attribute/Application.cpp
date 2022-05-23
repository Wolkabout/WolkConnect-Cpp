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

#include <random>

/**
 * This is the place where user input is required for running the example.
 * In here, you can enter the device credentials to successfully identify the device on the platform.
 * And also, the target platform path, and the SSL certificate that is used to establish a secure connection.
 */
const std::string DEVICE_KEY = "<DEVICE_KEY>";
const std::string DEVICE_PASSWORD = "<DEVICE_PASSWORD>";
const std::string PLATFORM_HOST = "ssl://INSERT_HOSTNAME:PORT";
const std::string CA_CERT_PATH = "/INSERT/PATH/TO/YOUR/CA.CRT/FILE";

/**
 * This is a function that will generate a random value for us.
 *
 * @return A new random value, in the range of 0 to 100.
 */
std::uint64_t generateRandomValue()
{
    // Here we will create the random engine and distribution
    static auto engine =
      std::mt19937(static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count()));
    static auto distribution = std::uniform_real_distribution<>(0, 100);

    // And generate a random value
    return static_cast<std::uint64_t>(distribution(engine));
}

int main(int /* argc */, char** /* argv */)
{
    // This is the logger setup. Here you can set up the level of logging you would like enabled.
    wolkabout::Logger::init(wolkabout::LogLevel::INFO, wolkabout::Logger::Type::CONSOLE);

    // Here we create the device that we are presenting as on the platform.
    auto device = wolkabout::Device(DEVICE_KEY, DEVICE_PASSWORD, wolkabout::OutboundDataMode::PUSH);

    // And here we create the wolk session
    auto wolk =
      wolkabout::connect::WolkSingle::newBuilder(device).host(PLATFORM_HOST).caCertPath(CA_CERT_PATH).buildWolkSingle();
    wolk->connect();

    // Now we will register a feed, see `wolkabout::FeedType` and `wolkabout::Unit` for more options.
    // Unit can also be custom, in case you have a custom unit on your Wolkabout instance.
    auto feed = wolkabout::Feed{"New Feed", "NF", wolkabout::FeedType::IN, wolkabout::Unit::NUMERIC};
    wolk->registerFeed(feed);

    // Now we will register an attribute. If the attribute is already present, the value will be updated.
    // And value that you pass here must be passed as a string, regardless of the `DataType`.
    // The `DataType` describes the way the data should be interpreted as.
    auto attribute = wolkabout::Attribute{"Device activation timestamp", wolkabout::DataType::NUMERIC,
                                          std::to_string(std::chrono::system_clock::now().time_since_epoch().count())};
    wolk->addAttribute(attribute);

    // And now we will periodically (and endlessly) send a random value for our new feed.
    while (true)
    {
        wolk->addReading("NF", generateRandomValue());
        std::this_thread::sleep_for(std::chrono::seconds(60));
        wolk->publish();
    }
    wolk->disconnect();
    return 0;
}

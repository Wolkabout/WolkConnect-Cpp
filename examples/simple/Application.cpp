/**
 * Copyright 2022 WolkAbout Technology s.r.o.
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

#include "core/utility/Logger.h"
#include "wolk/WolkBuilder.h"
#include "wolk/WolkSingle.h"

#include <random>

using namespace wolkabout::legacy;

/**
 * This is the place where user input is required for running the example.
 * In here, you can enter the device credentials to successfully identify the device on the platform.
 * And also, the target platform path.
 */
const std::string DEVICE_KEY = "<DEVICE_KEY>";
const std::string DEVICE_PASSWORD = "<DEVICE_PASSWORD>";
const std::string PLATFORM_HOST = "tcp://INSERT_HOSTNAME:PORT";

/**
 * This is a function that will generate a random Temperature value for us.
 *
 * @return A new Temperature value, in the range of -20 to 80.
 */
std::uint64_t generateRandomValue()
{
    // Here we will create the random engine and distribution
    static auto engine =
      std::mt19937(static_cast<std::uint32_t>(std::chrono::system_clock::now().time_since_epoch().count()));
    static auto distribution = std::uniform_real_distribution<>(-20, 80);

    // And generate a random value
    return static_cast<std::uint64_t>(distribution(engine));
}

int main(int /* argc */, char** /* argv */)
{
    // This is the logger setup. Here you can set up the level of logging you would like enabled.
    wolkabout::legacy::Logger::init(wolkabout::legacy::LogLevel::INFO, wolkabout::legacy::Logger::Type::CONSOLE);

    // Here we create the device that we are presenting as on the platform.
    auto device = wolkabout::Device(DEVICE_KEY, DEVICE_PASSWORD, wolkabout::OutboundDataMode::PUSH);

    // And here we create the wolk session
    auto wolk = wolkabout::connect::WolkSingle::newBuilder(device).host(PLATFORM_HOST).buildWolkSingle();
    wolk->connect();

    // And now we will periodically (and endlessly) send a random temperature value.
    while (true)
    {
        wolk->addReading("T", generateRandomValue());
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        wolk->publish();
    }
    return 0;
}

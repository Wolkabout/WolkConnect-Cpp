/**
 * Copyright 2022 Wolkabout Technology s.r.o.
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
#include "wolk/WolkMulti.h"

using namespace wolkabout;

class ExamplePlatformStatusListener : public wolkabout::connect::PlatformStatusListener
{
public:
    void platformStatus(wolkabout::ConnectivityStatus status) override
    {
        LOG(INFO) << "Received `platform_status`: '"
                  << (status == wolkabout::ConnectivityStatus::CONNECTED ? "CONNECTED" : "OFFLINE") << "'.";
    }
};

int main(int /* argc */, char** /* argv */)
{
    // This is the logger setup. Here you can set up the level of logging you would like enabled.
    wolkabout::Logger::init(wolkabout::LogLevel::TRACE, wolkabout::Logger::Type::CONSOLE);

    // Here we will create some devices
    auto deviceOne = wolkabout::Device{"FirstDevice", "", wolkabout::OutboundDataMode::PUSH};
    auto deviceTwo = wolkabout::Device{"SecondDevice", "", wolkabout::OutboundDataMode::PULL};
    auto deviceThree = wolkabout::Device{"ThirdDevice", "", wolkabout::OutboundDataMode::PUSH};

    // And now we can create the wolk session
    auto wolk = wolkabout::connect::WolkMulti::newBuilder({})
                  .host("tcp://localhost:1883")
                  .withFileTransfer("./files")
                  .withPlatformStatus(std::unique_ptr<ExamplePlatformStatusListener>(new ExamplePlatformStatusListener))
                  .withErrorProtocol(std::chrono::minutes{10})
                  .withRegistration()
                  .buildWolkMulti();
    wolk->connect();
    wolk->addReading(deviceOne.getKey(), "π", 3.14);
    wolk->publish();
    wolk->pullFeedValues(deviceTwo.getKey());
    wolk->pullParameters(deviceTwo.getKey());

    // Now we can sleep a little bit
    std::this_thread::sleep_for(std::chrono::seconds(5));
    wolk->addDevice(deviceThree);

    // Put them in an array
    auto devices = {deviceOne, deviceTwo, deviceThree};

    while (true)
    {
        // Now let's add a new device
        wolk->addReading(deviceThree.getKey(), "APM", 400);
        wolk->publish();

        // We can sleep again
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // And that's it
    return 0;
}

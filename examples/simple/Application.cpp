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

#include "core/model/Message.h"
#include "core/utilities/Logger.h"
#include "wolk/Wolk.h"

#include <chrono>
#include <memory>
#include <random>
#include <thread>

int main(int /* argc */, char** /* argv */)
{
    wolkabout::Logger::init(wolkabout::LogLevel::TRACE, wolkabout::Logger::Type::CONSOLE);

    wolkabout::Device device("ADCPSH", "BA7PVLD7UD", wolkabout::OutboundDataMode::PUSH);

    std::unique_ptr<wolkabout::Wolk> wolk =
      wolkabout::Wolk::newBuilder(device).host("integration5.wolkabout.com:2883").build();

    wolk->connect();

    auto engine = std::mt19937(static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()));
    auto distribution = std::uniform_real_distribution<>(-20, 80);

    while (true)
    {
        wolk->addReading("T", distribution(engine));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        wolk->publish();
    }

    wolk->disconnect();
    return 0;
}

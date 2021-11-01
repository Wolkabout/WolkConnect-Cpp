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

#include <chrono>
#include <memory>
#include <random>
#include <thread>

int main(int /* argc */, char** /* argv */)
{
    wolkabout::Device device("device_key", "some_password");

    std::unique_ptr<wolkabout::Wolk> wolk =
      wolkabout::Wolk::newBuilder(device).host("ssl://api-demo.wolkabout.com:8883").build();

    wolk->connect();

    std::default_random_engine engine;
    std::uniform_real_distribution<> distribution(-20, 80);

    while (true)
    {
        wolk->addReading("T", distribution(engine));

        wolk->publish();

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }

    return 0;
}

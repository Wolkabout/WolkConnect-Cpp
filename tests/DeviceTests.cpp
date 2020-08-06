/*
 * Copyright 2020 WolkAbout Technology s.r.o.
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

#define private public
#define protected public
#include "model/Device.h"
#undef private
#undef protected

#include <gtest/gtest.h>

class DeviceTests : public ::testing::Test
{
};

std::string randomString(size_t length)
{
    const char characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string generated(length, 0);
    std::generate_n(generated.begin(), length, [&]() -> char {
        const size_t max_index = (sizeof(characters) - 1);
        return characters[static_cast<unsigned long>(rand()) % max_index];
    });
    return generated;
}

TEST_F(DeviceTests, CtorTests)
{
    std::shared_ptr<wolkabout::Device> noActuators;
    EXPECT_NO_THROW(noActuators = std::make_shared<wolkabout::Device>("TEST1", "TEST1"));
    EXPECT_TRUE(noActuators->getActuatorReferences().empty());

    std::shared_ptr<wolkabout::Device> gotSomeActuators;
    const uint16_t count = rand() % 10 + 1;
    auto actuators = std::vector<std::string>();
    for (uint16_t i = 0; i < count; i++)
    {
        actuators.emplace_back(randomString(6));
    }

    EXPECT_NO_THROW(gotSomeActuators = std::make_shared<wolkabout::Device>("TEST2", "TEST2", actuators));
    EXPECT_FALSE(gotSomeActuators->getActuatorReferences().empty());
    EXPECT_EQ(count, gotSomeActuators->getActuatorReferences().size());
}

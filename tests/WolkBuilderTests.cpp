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
#include "WolkBuilder.h"
#undef private
#undef protected

#include <gtest/gtest.h>

class WolkBuilderTests : public ::testing::Test
{
};

TEST_F(WolkBuilderTests, ExampleTest)
{
    const auto& testDevice =
      std::make_shared<wolkabout::Device>("TEST_KEY", "TEST_PASSWORD", std::vector<std::string>{"A1", "A2", "A3"});

    const auto& builder = wolkabout::WolkBuilder(*testDevice);
}

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

#ifndef FEEDUPDATEHANDLER_H
#define FEEDUPDATEHANDLER_H

#include <string>

#include "core/Types.h"
namespace wolkabout
{
class FeedUpdateHandler
{
public:
    /**
     * @brief Feed Update handler callback<br>
     *        Must be implemented as non blocking<br>
     *        Must be implemented as thread safe
     * @param reference Feed reference
     * @param value Desired feed value
     */
    virtual void handleUpdate(const std::vector<Reading>) = 0;

    virtual ~FeedUpdateHandler() = default;
};
}    // namespace wolkabout

#endif

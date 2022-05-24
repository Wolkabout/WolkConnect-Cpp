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

#ifndef PARAMETERHANDLER_H
#define PARAMETERHANDLER_H

#include "core/Types.h"

#include <vector>

namespace wolkabout
{
namespace connect
{
/**
 * This interface describes an object that can receive parameter value updates.
 */
class ParameterHandler
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~ParameterHandler() = default;

    /**
     * This is the method that is invoked when new parameter values are obtained.
     *
     * @param deviceKey The key of the device which has received new parameter values.
     * @param parameters The vector containing new parameter values.
     */
    virtual void handleUpdate(const std::string& deviceKey, const std::vector<Parameter>& parameters) = 0;
};
}    // namespace connect
}    // namespace wolkabout

#endif

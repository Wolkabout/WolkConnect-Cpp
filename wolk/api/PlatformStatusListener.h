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

#ifndef WOLKABOUTCONNECTOR_PLATFORMSTATUSLISTENER_H
#define WOLKABOUTCONNECTOR_PLATFORMSTATUSLISTENER_H

#include "core/model/messages/PlatformStatusMessage.h"

namespace wolkabout
{
/**
 * This interface defines an object capable of receiving information that the connectivity status of the gateway has
 * changed.
 */
class PlatformStatusListener
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~PlatformStatusListener() = default;

    /**
     * This is the method that will be invoked once the connectivity status has changed.
     *
     * @param status The new status value.
     */
    virtual void platformStatus(ConnectivityStatus status) = 0;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_PLATFORMSTATUSLISTENER_H

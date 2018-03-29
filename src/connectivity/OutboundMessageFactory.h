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

#ifndef OUTBOUNDMESSAGEFACTORY_H
#define OUTBOUNDMESSAGEFACTORY_H

#include <memory>
#include <vector>

namespace wolkabout
{
class ActuatorStatus;
class Alarm;
class SensorReading;
class FirmwareUpdateResponse;
class FilePacketRequest;
class OutboundMessage;

class OutboundMessageFactory
{
public:
    virtual ~OutboundMessageFactory() = default;

    virtual std::shared_ptr<OutboundMessage> make(std::vector<std::shared_ptr<SensorReading>> sensorReadings) = 0;

    virtual std::shared_ptr<OutboundMessage> make(std::vector<std::shared_ptr<Alarm>> alarms) = 0;

    virtual std::shared_ptr<OutboundMessage> make(std::vector<std::shared_ptr<ActuatorStatus>> actuatorStatuses) = 0;

    virtual std::shared_ptr<OutboundMessage> make(const FirmwareUpdateResponse& firmwareUpdateResponse) = 0;

    virtual std::shared_ptr<OutboundMessage> make(const FilePacketRequest& filePacketRequest) = 0;

    virtual std::shared_ptr<OutboundMessage> makeFromFirmwareVersion(const std::string& firmwareVerion) = 0;
};
}    // namespace wolkabout

#endif

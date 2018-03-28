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

#ifndef JSONSINGLEOUTBOUNDMESSAGEFACTORY_H
#define JSONSINGLEOUTBOUNDMESSAGEFACTORY_H

#include "connectivity/OutboundMessageFactory.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class ActuatorStatus;
class Alarm;
class SensorReading;
class FirmwareUpdateResponse;
class FilePacketRequest;
class OutboundMessage;

class JsonSingleOutboundMessageFactory : public OutboundMessageFactory
{
public:
    JsonSingleOutboundMessageFactory(std::string deviceKey, std::map<std::string, std::string> sensorDelimiters);

    std::shared_ptr<OutboundMessage> make(std::vector<std::shared_ptr<SensorReading>> sensorReadings) override;
    std::shared_ptr<OutboundMessage> make(std::vector<std::shared_ptr<Alarm>> alarms) override;
    std::shared_ptr<OutboundMessage> make(std::vector<std::shared_ptr<ActuatorStatus>> actuatorStatuses) override;

    std::shared_ptr<OutboundMessage> make(const FirmwareUpdateResponse& firmwareUpdateResponse) override;

    std::shared_ptr<OutboundMessage> make(const FilePacketRequest& filePacketRequest) override;

    std::shared_ptr<OutboundMessage> makeFromFirmwareVersion(const std::string& firmwareVerion) override;

    std::shared_ptr<OutboundMessage> makePing() override;

private:
    std::string m_deviceKey;
    std::map<std::string, std::string> m_sensorDelimiters;

    static const constexpr char* SENSOR_READINGS_TOPIC_ROOT = "readings/";
    static const constexpr char* ALARMS_TOPIC_ROOT = "events/";
    static const constexpr char* ACTUATOR_STATUS_TOPIC_ROOT = "actuators/status/";
    static const constexpr char* FIRMWARE_UPDATE_STATUS_TOPIC_ROOT = "service/status/firmware/";
    static const constexpr char* FILE_HANDLING_STATUS_TOPIC_ROOT = "service/status/file/";
    static const constexpr char* FIRMWARE_VERSION_TOPIC_ROOT = "firmware/version/";
    static const constexpr char* PING_TOPIC_ROOT = "ping/";
};
}    // namespace wolkabout

#endif

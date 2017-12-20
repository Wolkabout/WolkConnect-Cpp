/*
 * Copyright 2017 WolkAbout Technology s.r.o.
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

#include "OutboundMessageFactory.h"
#include "model/ActuatorStatus.h"
#include "model/Alarm.h"
#include "model/OutboundMessage.h"
#include "model/SensorReading.h"
#include "model/FirmwareUpdateResponse.h"
#include "model/FileDownloadMqttResponse.h"
#include "utilities/json.hpp"

#include <memory>
#include <vector>

namespace wolkabout
{
using json = nlohmann::json;
/*** SENSOR READING ***/
void to_json(json& j, const SensorReading& p)
{
    if (p.getRtc() == 0)
    {
        j = json{{"data", p.getValue()}};
    }
    else
    {
        j = json{{"utc", p.getRtc()}, {"data", p.getValue()}};
    }
}

void to_json(json& j, const std::shared_ptr<SensorReading>& p)
{
	to_json(j, *p);
}
/*** SENSOR READING ***/

/*** ALARM ***/
void to_json(json& j, const Alarm& p)
{
    if (p.getRtc() == 0)
    {
        j = json{{"data", p.getValue()}};
    }
    else
    {
        j = json{{"utc", p.getRtc()}, {"data", p.getValue()}};
    }
}

void to_json(json& j, const std::shared_ptr<Alarm>& p)
{
	to_json(j, *p);
}
/*** ALARM ***/

/*** ACTUATOR STATUS ***/
void to_json(json& j, const ActuatorStatus& p)
{
	const std::string status = [&]() -> std::string {
		if (p.getState() == ActuatorStatus::State::READY)
		{
			return "READY";
		}
		else if (p.getState() == ActuatorStatus::State::BUSY)
		{
			return "BUSY";
		}
		else if (p.getState() == ActuatorStatus::State::ERROR)
		{
			return "ERROR";
		}

		return "ERROR";
	}();

	j = json{{"status", status}, {"value", p.getValue()}};
}

void to_json(json& j, const std::shared_ptr<ActuatorStatus>& p)
{
	to_json(j, *p);
}
/*** ACTUATOR STATUS ***/

/*** FIRMWARE UPDATE RESPONSE ***/
void to_json(json& j, const FirmwareUpdateResponse& p)
{
	const std::string status = [&]() -> std::string {
		if (p.getStatus() == FirmwareUpdateResponse::Status::OK)
		{
			return "OK";
		}
		else if (p.getStatus() == FirmwareUpdateResponse::Status::ERROR)
		{
			return "ERROR";
		}

		return "ERROR";
	}();

	if(!p.getErrorCode().null())
	{
		auto errorCode = static_cast<FirmwareUpdateResponse::ErrorCode>(p.getErrorCode());
		std::string value = std::to_string(static_cast<int>(errorCode));

		j = json{{"status", status}, {"value", value}};
	}
	else
	{
		j = json{{"status", status}};
	}
}

void to_json(json& j, const std::shared_ptr<FirmwareUpdateResponse>& p)
{
	to_json(j, *p);
}
/*** FIRMWARE UPDATE RESPONSE ***/

/*** FILE DOWNLOAD MQTT RESPONSE ***/
void to_json(json& j, const FileDownloadMqttResponse& p)
{
	const std::string status = [&]() -> std::string {
		if (p.getStatus() == FileDownloadMqttResponse::Status::OK)
		{
			return "OK";
		}
		else if (p.getStatus() == FileDownloadMqttResponse::Status::ERROR)
		{
			return "ERROR";
		}

		return "ERROR";
	}();

	if(!p.getErrorCode().null() && !p.getPackageNumber().null())
	{
		auto errorCode = static_cast<FileDownloadMqttResponse::ErrorCode>(p.getErrorCode());
		std::string value = std::to_string(static_cast<int>(errorCode));

		std::string packageNumber = std::to_string(static_cast<int>(p.getPackageNumber()));

		j = json{{"status", status}, {"value", value}, {"package", packageNumber}};
	}
	else if(!p.getErrorCode().null())
	{
		auto errorCode = static_cast<FileDownloadMqttResponse::ErrorCode>(p.getErrorCode());
		std::string value = std::to_string(static_cast<int>(errorCode));

		j = json{{"status", status}, {"value", value}};
	}
	else if(!p.getPackageNumber().null())
	{
		std::string packageNumber = std::to_string(static_cast<int>(p.getPackageNumber()));

		j = json{{"status", status}, {"package", packageNumber}};
	}
	else
	{
		j = json{{"status", status}};
	}
}

void to_json(json& j, const std::shared_ptr<FileDownloadMqttResponse>& p)
{
	to_json(j, *p);
}
/*** FILE DOWNLOAD MQTT RESPONSE ***/


std::shared_ptr<OutboundMessage> OutboundMessageFactory::make(
  const std::string& deviceKey, std::vector<std::shared_ptr<SensorReading>> sensorReadings)
{
    if (sensorReadings.size() == 0)
    {
        return nullptr;
    }

    const json jPayload(sensorReadings);
    const std::string payload = jPayload.dump();
    const std::string topic = SENSOR_READINGS_TOPIC_ROOT + deviceKey + "/" + sensorReadings.front()->getReference();

    return std::make_shared<OutboundMessage>(payload, topic, sensorReadings.size());
}

std::shared_ptr<OutboundMessage> OutboundMessageFactory::make(const std::string& deviceKey,
                                                              std::vector<std::shared_ptr<Alarm>> alarms)
{
    if (alarms.size() == 0)
    {
        return nullptr;
    }

    const json jPayload(alarms);
    const std::string payload = jPayload.dump();
    const std::string topic = ALARMS_TOPIC_ROOT + deviceKey + "/" + alarms.front()->getReference();

    return std::make_shared<OutboundMessage>(payload, topic, alarms.size());
}

std::shared_ptr<OutboundMessage> OutboundMessageFactory::make(
  const std::string& deviceKey, std::vector<std::shared_ptr<ActuatorStatus>> actuatorStatuses)
{
    if (actuatorStatuses.size() == 0)
    {
        return nullptr;
    }

    const json jPayload(actuatorStatuses.front());
    const std::string payload = jPayload.dump();
	const std::string topic = ACTUATOR_STATUS_TOPIC_ROOT + deviceKey + "/" + actuatorStatuses.front()->getReference();

    /* Currently supported protocol (JSON_SINGLE) allows only 1 ActuatorStatus per OutboundMessage, hence 'magic' number
     * 1 below */
    return std::make_shared<OutboundMessage>(payload, topic, 1);
}

std::shared_ptr<OutboundMessage> OutboundMessageFactory::make(
		const std::string& deviceKey, const FirmwareUpdateResponse& firmwareUpdateResponse)
{
	const json jPayload{firmwareUpdateResponse};
	const std::string payload = jPayload.dump();
	const std::string topic = FIRMWARE_UPDATE_STATUS_TOPIC_ROOT + deviceKey;

	return std::make_shared<OutboundMessage>(payload, topic, 1);
}

std::shared_ptr<OutboundMessage> OutboundMessageFactory::make(
		const std::string& deviceKey, const FileDownloadMqttResponse& fileDownloadMqttResponse)
{
	const json jPayload{fileDownloadMqttResponse};
	const std::string payload = jPayload.dump();
	const std::string topic = MQTT_FILE_HANDING_STATUS_TOPIC_ROOT + deviceKey;

	return std::make_shared<OutboundMessage>(payload, topic, 1);
}
}

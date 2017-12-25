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

#include "connectivity/mqtt/JsonDtoParser.h"
#include "utilities/json.hpp"

#include "model/ActuatorCommand.h"
#include "model/ActuatorStatus.h"
#include "model/Alarm.h"
#include "model/SensorReading.h"
#include "model/FirmwareUpdateCommand.h"
#include "model/FileDownloadMqttCommand.h"
#include "model/FileDownloadUrlCommand.h"
#include "utilities/StringUtils.h"

#include <string>

namespace wolkabout
{
using nlohmann::json;

/*** ACTUATOR COMMAND ***/
void to_json(json& j, const ActuatorCommand& p)
{
    j = json{{"command", p.getType() == ActuatorCommand::Type::SET ? "SET" : "STATUS"}, {"value", p.getValue()}};
}

void from_json(const json& j, ActuatorCommand& p)
{
    const std::string type = j.at("command").get<std::string>();
    const std::string value = [&]() -> std::string {
        if (j.find("value") != j.end())
        {
            return j.at("value").get<std::string>();
        }
        else
        {
            return "";
        }
    }();

    p = ActuatorCommand(type == "SET" ? ActuatorCommand::Type::SET : ActuatorCommand::Type::STATUS, "", value);
}

bool JsonParser::fromJson(const std::string& jsonString, ActuatorCommand& actuatorCommandDto)
{
    try
    {
        json j = json::parse(jsonString);
        actuatorCommandDto = j;
    }
    catch (...)
    {
        return false;
    }

    return true;
}
/*** ACTUATOR COMMAND ***/

/*** FIRMWARE UPDATE COMMAND ***/
void from_json(const json& j, FirmwareUpdateCommand& p)
{
	const std::string typeStr = j.at("command").get<std::string>();

	FirmwareUpdateCommand::Type type;
	if(typeStr == "INSTALL")
	{
		type = FirmwareUpdateCommand::Type::INSTALL;
	}
	else if(typeStr == "ABORT")
	{
		type = FirmwareUpdateCommand::Type::ABORT;
	}
	else
	{
		type = FirmwareUpdateCommand::Type::INIT;
	}


	p = FirmwareUpdateCommand(type);
}

bool JsonParser::fromJson(const std::string& jsonString, FirmwareUpdateCommand& firmwareUpdateCommandDto)
{
	try
	{
		if(StringUtils::startsWith(jsonString, "{"))
		{
			json j = json::parse(jsonString);
			firmwareUpdateCommandDto = j;
		}
		else
		{
			FirmwareUpdateCommand::Type type;
			if(jsonString == "INSTALL")
			{
				type = FirmwareUpdateCommand::Type::INSTALL;
			}
			else if(jsonString == "ABORT")
			{
				type = FirmwareUpdateCommand::Type::ABORT;
			}
			else
			{
				type = FirmwareUpdateCommand::Type::INIT;
			}

			firmwareUpdateCommandDto = FirmwareUpdateCommand(type);
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}
/*** FIRMWARE UPDATE COMMAND ***/

/*** FILE DOWNLOAD MQTT COMMAND ***/
void from_json(const json& j, FileDownloadMqttCommand& p)
{
	const std::string typeStr = j.at("command").get<std::string>();

	FileDownloadMqttCommand::Type type;
	if(typeStr == "END")
	{
		type = FileDownloadMqttCommand::Type::END;
	}
	else if(typeStr == "STATUS")
	{
		type = FileDownloadMqttCommand::Type::STATUS;
	}
	else
	{
		type = FileDownloadMqttCommand::Type::INIT;
	}


	WolkOptional<std::string> name{};
	try
	{
		name = j.at("name").get<std::string>();
	}
	catch (...) {}

	WolkOptional<int> size{};
	try
	{
		size = std::stoi(j.at("size").get<std::string>());
	}
	catch (...) {}

	WolkOptional<std::string> hash{};
	try
	{
		hash = j.at("hash").get<std::string>();
	}
	catch (...) {}

	p = FileDownloadMqttCommand(type, name, size, hash);
}

bool JsonParser::fromJson(const std::string& jsonString, FileDownloadMqttCommand& fileDownloadMqttCommandDto)
{
	try
	{
		if(StringUtils::startsWith(jsonString, "{"))
		{
			json j = json::parse(jsonString);
			fileDownloadMqttCommandDto = j;
		}
		else
		{
			FileDownloadMqttCommand::Type type;
			if(jsonString == "END")
			{
				type = FileDownloadMqttCommand::Type::END;
			}
			else if(jsonString == "STATUS")
			{
				type = FileDownloadMqttCommand::Type::STATUS;
			}
			else
			{
				type = FileDownloadMqttCommand::Type::INIT;
			}

			fileDownloadMqttCommandDto = FileDownloadMqttCommand(type);
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}
/*** FILE DOWNLOAD MQTT COMMAND ***/

/*** FILE DOWNLOAD URL COMMAND ***/
void from_json(const json& j, FileDownloadUrlCommand& p)
{
	const std::string typeStr = j.at("command").get<std::string>();

	FileDownloadUrlCommand::Type type;
	if(typeStr == "END")
	{
		type = FileDownloadUrlCommand::Type::END;
	}
	else if(typeStr == "STATUS")
	{
		type = FileDownloadUrlCommand::Type::STATUS;
	}
	else
	{
		type = FileDownloadUrlCommand::Type::INIT;
	}

	WolkOptional<std::string> url{};
	try
	{
		url = j.at("url").get<std::string>();
	}
	catch (...) {}

	p = FileDownloadUrlCommand(type, url);
}

bool JsonParser::fromJson(const std::string& jsonString, FileDownloadUrlCommand& fileDownloadUrlCommandDto)
{
	try
	{
		if(StringUtils::startsWith(jsonString, "{"))
		{
			json j = json::parse(jsonString);
			fileDownloadUrlCommandDto = j;
		}
		else
		{
			FileDownloadUrlCommand::Type type;
			if(jsonString == "END")
			{
				type = FileDownloadUrlCommand::Type::END;
			}
			else if(jsonString == "STATUS")
			{
				type = FileDownloadUrlCommand::Type::STATUS;
			}
			else
			{
				type = FileDownloadUrlCommand::Type::INIT;
			}

			fileDownloadUrlCommandDto = FileDownloadUrlCommand(type);
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}
/*** FILE DOWNLOAD URL COMMAND ***/
}

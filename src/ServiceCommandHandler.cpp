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

#include "ServiceCommandHandler.h"
#include "BinaryDataListener.h"
#include "FirmwareUpdateCommandListener.h"
#include "FileDownloadMqttCommandListener.h"

namespace wolkabout
{
void ServiceCommandHandler::handleBinaryData(const BinaryData& binaryData)
{
	if(auto handler = m_binaryDataHandler.lock())
	{
		handler->handleBinaryData(binaryData);
	}
}

void ServiceCommandHandler::handleFirmwareUpdateCommand(const FirmwareUpdateCommand& firmwareUpdateCommand)
{
	if(auto handler = m_firmwareUpdateCommandHandler.lock())
	{
		handler->handleFirmwareUpdateCommand(firmwareUpdateCommand);
	}
}

void ServiceCommandHandler::handleFileDownloadMqttCommand(const FileDownloadMqttCommand& fileDownloadMqttCommand)
{
	if(auto handler = m_fileDownloadMqttCommandHandler.lock())
	{
		handler->handleFileDownloadMqttCommand(fileDownloadMqttCommand);
	}
}

void ServiceCommandHandler::setBinaryDataHandler(std::weak_ptr<BinaryDataListener> handler)
{
	m_binaryDataHandler = handler;
}

void ServiceCommandHandler::setFileDownloadMqttCommandHandler(std::weak_ptr<FileDownloadMqttCommandListener> handler)
{
	m_fileDownloadMqttCommandHandler = handler;
}

void ServiceCommandHandler::setFirmwareUpdateCommandHandler(std::weak_ptr<FirmwareUpdateCommandListener> handler)
{
	m_firmwareUpdateCommandHandler = handler;
}

}

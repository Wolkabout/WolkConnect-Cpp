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

#ifndef SERVICECOMMANDHANDLER_H
#define SERVICECOMMANDHANDLER_H

#include <memory>

namespace wolkabout
{

class BinaryData;
class FirmwareUpdateCommand;
class FileDownloadMqttCommand;
class FileDownloadUrlCommand;
class BinaryDataListener;
class FirmwareUpdateCommandListener;
class FileDownloadMqttCommandListener;
class FileDownloadUrlCommandListener;

class ServiceCommandHandler
{
public:
	void handleBinaryData(const BinaryData& binaryData);

	void handleFirmwareUpdateCommand(const FirmwareUpdateCommand& firmwareUpdateCommand);

	void handleFileDownloadMqttCommand(const FileDownloadMqttCommand& fileDownloadMqttCommand);

	void handleFileDownloadUrlCommand(const FileDownloadUrlCommand& fileDownloadUrlCommand);


	void setBinaryDataHandler(std::weak_ptr<BinaryDataListener> handler);

	void setFirmwareUpdateCommandHandler(std::weak_ptr<FirmwareUpdateCommandListener> handler);

	void setFileDownloadMqttCommandHandler(std::weak_ptr<FileDownloadMqttCommandListener> handler);

	void setFileDownloadUrlCommandHandler(std::weak_ptr<FileDownloadUrlCommandListener> handler);

private:
	std::weak_ptr<BinaryDataListener> m_binaryDataHandler;
	std::weak_ptr<FileDownloadMqttCommandListener> m_fileDownloadMqttCommandHandler;
	std::weak_ptr<FirmwareUpdateCommandListener> m_firmwareUpdateCommandHandler;
	std::weak_ptr<FileDownloadUrlCommandListener> m_fileDownloadUrlCommandHandler;
};

}

#endif // SERVICECOMMANDHANDLER_H

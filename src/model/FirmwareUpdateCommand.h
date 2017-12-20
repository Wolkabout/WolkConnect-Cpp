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

#ifndef FIRMWAREUPDATECOMMAND_H
#define FIRMWAREUPDATECOMMAND_H

#include <string>

namespace wolkabout
{
class FirmwareUpdateCommand
{
public:
	enum class Type
	{
		INIT,
		INSTALL,
		ABORT
	};

	FirmwareUpdateCommand();
	explicit FirmwareUpdateCommand(FirmwareUpdateCommand::Type type);

	virtual ~FirmwareUpdateCommand() = default;

	FirmwareUpdateCommand::Type getType() const;

private:
	FirmwareUpdateCommand::Type m_type;
};
}

#endif

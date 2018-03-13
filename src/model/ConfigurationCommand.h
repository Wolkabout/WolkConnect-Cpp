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

#ifndef CONFIGURATIONCOMMAND_H
#define CONFIGURATIONCOMMAND_H

#include <map>
#include <string>

namespace wolkabout
{
class ConfigurationCommand
{
public:
    enum class Type
    {
        CURRENT,
        SET
    };

    ConfigurationCommand() = default;
    ConfigurationCommand(ConfigurationCommand::Type type, std::map<std::string, std::string> values);

    virtual ~ConfigurationCommand() = default;

    ConfigurationCommand::Type getType() const;

    const std::map<std::string, std::string>& getValues() const;

private:
    Type m_type;
    std::map<std::string, std::string> m_values;
};
}    // namespace wolkabout

#endif

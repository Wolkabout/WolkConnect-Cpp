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

#include "BinaryData.h"
#include "utilities/StringUtils.h"
#include <stdexcept>

namespace
{
const short SHA_256_HASH_BYTE_LENGTH = 32;
const short SHA_256_HASH_CHAR_LENGTH = 2 * SHA_256_HASH_BYTE_LENGTH;
}

namespace wolkabout
{

BinaryData::BinaryData() : m_value(""), m_data(""), m_hash(""), m_previousHash("")
{
}

BinaryData::BinaryData(const std::string& value) : m_value(value)
{
	if(value.length() <= 2 * SHA_256_HASH_CHAR_LENGTH)
	{
		throw new std::invalid_argument("Binary data size is smaller than required to fit standard data package");
	}

	m_previousHash = m_value.substr(0, SHA_256_HASH_CHAR_LENGTH);
	m_data = m_value.substr(SHA_256_HASH_CHAR_LENGTH, m_value.length() - SHA_256_HASH_CHAR_LENGTH);
	m_hash = m_value.substr(SHA_256_HASH_CHAR_LENGTH + m_data.length(), SHA_256_HASH_CHAR_LENGTH);
}

const std::string& BinaryData::getData() const
{
	return m_data;
}

const std::string& BinaryData::getHash() const
{
	return m_hash;
}

bool BinaryData::valid() const
{
	const std::string hash = StringUtils::hashSHA256(m_previousHash + m_data);
	return hash == m_hash;
}

bool BinaryData::validatePrevious() const
{
	return validatePrevious(StringUtils::hashSHA256(""));
}

bool BinaryData::validatePrevious(const std::string& previousHash) const
{
	return m_previousHash == previousHash;
}

}

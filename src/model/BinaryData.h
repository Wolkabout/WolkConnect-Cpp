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

#ifndef BINARYDATA_H
#define BINARYDATA_H

#include <string>

namespace wolkabout
{
class BinaryData
{
public:
	BinaryData();

	/**
	 * @brief BinaryData Constructs the object using binary value
	 * Throws std::invalid_argument if the length binary value is not
	 * big enough to contain valid data
	 * @param value Binary data
	 */
	BinaryData(const std::string& value);

	virtual ~BinaryData() = default;

	/**
	 * @brief getData
	 * @return data part of the binary package
	 */
	const std::string& getData() const;

	/**
	 * @brief getHash
	 * @return hash part of the binary package
	 */
	const std::string& getHash() const;

	/**
	 * @brief valid Validates the package using its hash
	 * @return true if package is valid, false otherwise
	 */
	bool valid() const;

	/**
	 * @brief validatePrevious Validates that param matches the previous hash
	 * part of the binary package
	 * @param previousHash Previous hash to validate against
	 * @return true if previous hash is valid, false otherwise
	 */
	bool validatePrevious(const std::string& previousHash) const;

	/**
	 * @brief validatePrevious Validates that the previous hash part of the
	 * binary package matches hash of an empty string
	 * Used when package is first in order and no previous hash exists
	 * @return true if previous hash is valid, false otherwise
	 */
	bool validatePrevious() const;

	static const short SHA_256_HASH_BYTE_LENGTH = 32;
	static const short SHA_256_HASH_CHAR_LENGTH = 2 * SHA_256_HASH_BYTE_LENGTH;

private:
	std::string m_value;

	std::string m_data;
	std::string m_hash;
	std::string m_previousHash;
};
}

#endif // BINARYDATA_H

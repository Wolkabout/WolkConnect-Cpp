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

#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <string>
#include <sstream>
#include <stdexcept>

namespace wolkabout
{
class BinaryData;

class FileHandler
{
public:
	enum class StatusCode
	{
		OK = 0,
		PACKAGE_HASH_NOT_VALID,
		PREVIOUS_PACKAGE_HASH_NOT_VALID,
		TRANSFER_NOT_INITIATED,
		TRANSFER_NOT_COMPLETED,
		FILE_HASH_NOT_VALID,
		FILE_HANDLING_ERROR,
		FILE_SIZE_NOT_SUPPORTED
	};

	FileHandler(const std::string& path, int maxFileSize, int maxPackageSize);

	virtual ~FileHandler() = default;

	FileHandler::StatusCode prepare(const std::string& fileName, int fileSize, const std::string& fileHash,
									int& packageSize, int& packageCount);

	void clear();

	FileHandler::StatusCode handleData(const BinaryData& binaryData, int& packageNumber);

	FileHandler::StatusCode validateFile();

	FileHandler::StatusCode saveFile();

private:
	const std::string m_path;
	const int m_maxFileSize;
	const int m_maxPackageSize;

	std::string m_currentFileName;
	int m_currentPackageSize;
	int m_currentPackageCount;
	std::string m_currentFileHash;

	std::stringstream m_currentPackageData;
	int m_currentPackageIndex;
	std::string m_previousPackageHash;
};
}


#endif // FILEHANDLER_H

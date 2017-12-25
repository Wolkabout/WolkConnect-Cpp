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

#include "FileHandler.h"
#include "model/BinaryData.h"
#include "utilities/StringUtils.h"
#include "utilities/FileSystemUtils.h"
#include <cmath>

namespace wolkabout
{

FileHandler::FileHandler(const std::string& path, int maxFileSize, int maxPackageSize) :
	m_path{path}, m_maxFileSize{maxFileSize}, m_maxPackageSize{maxPackageSize},
	m_currentFileName{""}, m_currentPackageSize{-1}, m_currentPackageCount{-1}, m_currentFileHash{""},
	m_currentPackageData{""}, m_currentPackageIndex{-1}, m_previousPackageHash{""}
{
}

FileHandler::StatusCode FileHandler::prepare(const std::string& fileName, int fileSize,
											 const std::string& fileHash, int& packageSize,
											 int& packageCount)
{
	if(fileSize > m_maxFileSize)
	{
		return FileHandler::StatusCode::FILE_SIZE_NOT_SUPPORTED;
	}

	auto count = fileSize / (m_maxPackageSize - (2 * BinaryData::SHA_256_HASH_BYTE_LENGTH));

	m_currentPackageCount = static_cast<int>(std::ceil(count));
	packageCount = m_currentPackageCount;
	packageSize = m_maxPackageSize;

	m_currentFileName = fileName;
	m_currentPackageSize = packageSize;
	m_currentPackageCount = packageCount;
	m_currentFileHash = fileHash;

	m_currentPackageData.str("");
	m_currentPackageIndex = 0;
	m_previousPackageHash = "";

	return FileHandler::StatusCode::OK;
}

void FileHandler::clear()
{
	m_currentFileName = "";
	m_currentPackageSize = -1;
	m_currentPackageCount = -1;
	m_currentFileHash = "";
	m_currentPackageData.str("");
	m_currentPackageIndex = -1;
	m_previousPackageHash = "";
}

FileHandler::StatusCode FileHandler::handleData(const BinaryData& binaryData, int& packageNumber)
{
	packageNumber = m_currentPackageIndex;

	if(m_currentPackageIndex == -1)
	{
		return FileHandler::StatusCode::TRANSFER_NOT_INITIATED;
	}

	if(!binaryData.valid())
	{
		return FileHandler::StatusCode::PACKAGE_HASH_NOT_VALID;
	}

	if(m_previousPackageHash.empty())
	{
		if(!binaryData.validatePrevious())
		{
			return FileHandler::StatusCode::PREVIOUS_PACKAGE_HASH_NOT_VALID;
		}
	}
	else
	{
		if(!binaryData.validatePrevious(m_previousPackageHash))
		{
			return FileHandler::StatusCode::PREVIOUS_PACKAGE_HASH_NOT_VALID;
		}
	}

	m_currentPackageData << binaryData.getData();
	m_previousPackageHash = binaryData.getHash();
	++m_currentPackageIndex;

	return FileHandler::StatusCode::OK;
}

FileHandler::StatusCode FileHandler::validateFile()
{
	if(m_currentPackageIndex != m_currentPackageCount)
	{
		return FileHandler::StatusCode::TRANSFER_NOT_COMPLETED;
	}

	if(m_currentFileHash == StringUtils::hashSHA256(m_currentPackageData.str()))
	{
		return FileHandler::StatusCode::OK;
	}
	else
	{
		return FileHandler::StatusCode::FILE_HASH_NOT_VALID;
	}
}

FileHandler::StatusCode FileHandler::saveFile()
{
	const std::string filePath = m_path + "/" + m_currentFileName;

	if(FileSystemUtils::createBinaryFileWithContent(filePath, m_currentPackageData.str()))
	{
		return FileHandler::StatusCode::OK;
	}
	else
	{
		return FileHandler::StatusCode::FILE_HANDLING_ERROR;
	}
}

}

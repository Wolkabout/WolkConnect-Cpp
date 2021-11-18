/**
 * Copyright 2021 WolkAbout Technology s.r.o.
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

#define private public
#define protected public
#include "repository/SQLiteFileRepository.h"
#undef private
#undef protected

#include <gtest/gtest.h>

#include <iostream>

class SQLiteFileRepositoryTests : public ::testing::Test
{
public:
    void TearDown()
    {
        if (std::remove(fileName.c_str()) == 0)
            std::cout << "SQLiteFileRepositoryTests: Successfully cleaned up repository file." << std::endl;
    }

    static std::string fileName;
};

std::string SQLiteFileRepositoryTests::fileName = "TEST_DUMP_FILE";

TEST_F(SQLiteFileRepositoryTests, ConstructorTest)
{
    EXPECT_NO_THROW(wolkabout::SQLiteFileRepository repository(fileName));
}

TEST_F(SQLiteFileRepositoryTests, HappyFlowsTests)
{
    std::unique_ptr<wolkabout::SQLiteFileRepository> repository;
    ASSERT_NO_THROW(repository =
                      std::unique_ptr<wolkabout::SQLiteFileRepository>(new wolkabout::SQLiteFileRepository(fileName)));

    std::unique_ptr<std::vector<std::string>> fileNames;
    ASSERT_NO_THROW(fileNames = repository->getAllFileNames());
    EXPECT_EQ(fileNames->size(), 0);

    EXPECT_EQ(repository->getFileInfo("NOT_EXISTING_FILE"), nullptr);

    const auto& fileInfo1 = wolkabout::FileInfo("TEST_FILE1", "HASH1", "/path/to/file1");
    const auto& fileInfo2 = wolkabout::FileInfo("TEST_FILE2", "HASH2", "/path/to/file2");
    const auto& fileInfo3 = wolkabout::FileInfo("TEST_FILE3", "HASH3", "/path/to/file3");

    EXPECT_NO_THROW(repository->store(fileInfo1));

    EXPECT_NO_THROW(repository->store(fileInfo2));
    // This one is expected to update now
    EXPECT_NO_THROW(repository->store(fileInfo2));

    EXPECT_NO_THROW(repository->remove("NOT_EXISTING_FILE"));

    EXPECT_NO_THROW(repository->remove(fileInfo1.name));
    EXPECT_NO_THROW(repository->store(fileInfo3));

    EXPECT_NE(repository->getFileInfo(fileInfo3.name), nullptr);
    EXPECT_EQ(repository->getAllFileNames()->size(), 2);

    EXPECT_NO_THROW(repository->removeAll());
    EXPECT_EQ(repository->getAllFileNames()->size(), 0);
}

// Sorry, but this test is impossible to execute
// All catches don't return anything by which I can verify it caught the error
// And only way to induce a catch is to mess with the statement, which always results in SIGSEGV
/*
TEST_F(SQLiteFileRepositoryTests, CatchErrors)
{
    std::unique_ptr<wolkabout::SQLiteFileRepository> repository;
    ASSERT_NO_THROW(repository =
                      std::unique_ptr<wolkabout::SQLiteFileRepository>(new wolkabout::SQLiteFileRepository(fileName)));

    repository->m_session = nullptr;

    // These all should enter catch statement, since statement will work with nullptr session.
    EXPECT_EQ(repository->getFileInfo("NOT_EXISTING_FILE"), nullptr);
    EXPECT_NE(repository->getAllFileNames(), nullptr);

    const auto& info = wolkabout::FileInfo("TEST_FILE", "TEST_HASH", "TEST_PATH");
    EXPECT_NO_THROW(repository->store(info));

    EXPECT_NO_THROW(repository->removeAll());
    EXPECT_NO_THROW(repository->containsInfoForFile("TEST_FILE"));
}
*/

/**
 * Copyright 2021 Wolkabout s.r.o.
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

#ifndef WOLKABOUTCONNECTOR_FILELISTENER_H
#define WOLKABOUTCONNECTOR_FILELISTENER_H

#include <string>

namespace wolkabout
{
/**
 * This is an interface meant to define an object that can receive information about added and removed files by the
 * FileManagementService.
 */
class FileListener
{
public:
    /**
     * Default virtual destructor.
     */
    virtual ~FileListener() = default;

    /**
     * This is a method that will be invoked once a file has been obtained.
     *
     * @param fileName The name of the new file.
     * @param absolutePath The absolute path of the new file.
     */
    virtual void onAddedFile(const std::string& fileName, const std::string& absolutePath) = 0;

    /**
     * This is a method that will be invoked once a file has been removed.
     *
     * @param fileName The name of the deleted file.
     */
    virtual void onRemovedFile(const std::string& fileName) = 0;
};
}    // namespace wolkabout

#endif    // WOLKABOUTCONNECTOR_FILELISTENER_H
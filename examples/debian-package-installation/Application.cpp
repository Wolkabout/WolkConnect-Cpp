/**
 * Copyright 2022 Wolkabout Technology s.r.o.
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

#include "core/utilities/Logger.h"
#include "wolk/service/firmware_update/debian/apt/APTPackageInstaller.h"

using namespace wolkabout;
using namespace wolkabout::connect;

int main()
{
    Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE);
    std::mutex mutex;
    std::condition_variable conditionVariable;

    APTPackageInstaller installer;
    if (installer.installPackage("/home/astrihale/release-v5.0.0-prerelease/wolkgateway_5.0.0-prerelease_amd64.deb",
                                 [&](const std::string& debianPackagePath, InstallationResult result) {
                                     conditionVariable.notify_one();
                                     LOG(INFO) << "Received result for installation of '" << debianPackagePath
                                               << "' -> '" << toString(result) << "'.";
                                 }))
    {
        std::unique_lock<std::mutex> lock{mutex};
        conditionVariable.wait_for(lock, std::chrono::seconds{120});
    }

    return 0;
}

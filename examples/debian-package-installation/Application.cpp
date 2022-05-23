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
#include "wolk/WolkBuilder.h"
#include "wolk/WolkSingle.h"
#include "wolk/service/firmware_update/debian/DebianPackageInstaller.h"

/**
 * This is the place where user input is required for running the example.
 * In here, you can enter the device credentials to successfully identify the device on the platform.
 * And also, the target platform path, and the SSL certificate that is used to establish a secure connection.
 */
const std::string DEVICE_KEY = "AWC";
const std::string DEVICE_PASSWORD = "0ZY4R8VSSD";
const std::string PLATFORM_HOST = "ssl://integration5.wolkabout.com:8883";
const std::string CA_CERT_PATH = "/INSERT/PATH/TO/YOUR/CA.CRT/FILE";
const std::string FILE_MANAGEMENT_LOCATION = "./files";

// Declare the namespaces we're going to use.
using namespace wolkabout;
using namespace wolkabout::connect;

int main()
{
    Logger::init(LogLevel::TRACE, Logger::Type::CONSOLE);

    auto device = wolkabout::Device{DEVICE_KEY, DEVICE_PASSWORD, wolkabout::OutboundDataMode::PUSH};

    auto wolk = std::unique_ptr<WolkSingle>{};
    {
        // Create the debian package installer
        auto installer = [&] {
            auto aptInstaller = std::unique_ptr<APTPackageInstaller>{new APTPackageInstaller};
            auto systemdManager = std::unique_ptr<SystemdServiceInterface>{new SystemdServiceInterface};
            return std::unique_ptr<DebianPackageInstaller>{
              new DebianPackageInstaller{"wolkgateway", std::move(aptInstaller), std::move(systemdManager)}};
        }();
        installer->start();

        // Create a wolk instance
        wolk = wolkabout::connect::WolkBuilder(device)
                 .host(PLATFORM_HOST)
                 .caCertPath(CA_CERT_PATH)
                 .withFileTransfer(FILE_MANAGEMENT_LOCATION)
                 .withFirmwareUpdate(std::move(installer))
                 .buildWolkSingle();
    }

    wolk->connect();
    while (true)
        std::this_thread::sleep_for(std::chrono::seconds{1});
    return 0;
}

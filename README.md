# WolkConnector C++
WolkAbout C++11 Connector library for connecting devices to WolkAbout IoT platform.

Supported protocol(s):
* JSON_SINGLE

Prerequisite
------
Following tools/libraries are required in order to build WolkAbout C++ connector

* cmake - version 3.5 or later
* autotools
* autoconf
* m4
* zlib1g-dev

Former can be installed on Debian based system from terminal by invoking

`apt-get install autotools-dev autoconf m4 zlib1g-dev cmake`

Afterwards dependencies are built, and Makefile build system is generated by invoking
`./configure`

Generated build system is located inside 'out' directory


WolkAbout C++ Connector library, and example are built from 'out' directory by invoking
`make` in terminal

Example Usage
-------------
**Establishing connection with WolkAbout IoT platform:**
```cpp
wolkabout::Device device("DEVICE_KEY", "DEVICE_PASSWORD", {"ACTUATOR_REFERENCE_ONE", "ACTUATOR_REFERENCE_TWO"});

std::unique_ptr<wolkabout::Wolk> wolk =
  wolkabout::Wolk::newBuilder(device)
    .actuationHandler([](const std::string& reference, const std::string& value) -> void {
        // TODO Invoke your code which activates your actuator.

        std::cout << "Actuation request received - Reference: " << reference << " value: " << value << std::endl;
    })
    .actuatorStatusProvider([](const std::string& reference) -> wolkabout::ActuatorStatus {
        // TODO Invoke code which reads the state of the actuator.

        if (reference == "ACTUATOR_REFERENCE_ONE") {
            return wolkabout::ActuatorStatus("65", wolkabout::ActuatorStatus::State::READY);
        } else if (reference == "ACTUATOR_REFERENCE_TWO") {
            return wolkabout::ActuatorStatus("false", wolkabout::ActuatorStatus::State::READY);
        }

        return wolkabout::ActuatorStatus("", wolkabout::ActuatorStatus::State::READY);
    })
    .build();

    wolk->connect();
```

**Publishing sensor readings:**
```cpp
wolk->addSensorReading("TEMPERATURE_REF", 23.4);
wolk->addSensorReading("BOOL_SENSOR_REF", true);
```

**Publishing actuator statuses:**
```cpp
wolk->publishActuatorStatus("ACTUATOR_REFERENCE_ONE ");
```
This will invoke the ActuationStatusProvider to read the actuator status,
and publish actuator status.

**Publishing events:**
```cpp
wolk->addAlarm("ALARM_REF", "ALARM_MESSAGE_FROM_CONNECTOR");
```

**Data publish strategy:**

Sensor readings, and alarms are pushed to WolkAbout IoT platform on demand by calling
```cpp
wolk->publish();
```

Whereas actuator statuses are published automatically by calling:

```cpp
wolk->publishActuatorStatus("ACTUATOR_REFERENCE_ONE ");
```

**Disconnecting from the platform:**
```cpp
wolk->disconnect();
```

**Data persistence:**

WolkAbout C++ Connector provides mechanism for persisting data in situations where readings can not be sent to WolkAbout IoT platform.

Persisted readings are sent to WolkAbout IoT platform once connection is established.
Data persistence mechanism used **by default** stores data in-memory.

In cases when provided in-memory persistence is suboptimal, one can use custom persistence by implementing wolkabout::Persistence interface,
and forwarding it to builder in following manner:

```cpp
wolkabout::Device device("DEVICE_KEY", "DEVICE_PASSWORD", {"ACTUATOR_REFERENCE_ONE", "ACTUATOR_REFERENCE_TWO"});

std::unique_ptr<wolkabout::Wolk> wolk =
  wolkabout::Wolk::newBuilder(device)
    .actuationHandler([](const std::string& reference, const std::string& value) -> void {
        std::cout << "Actuation request received - Reference: " << reference << " value: " << value << std::endl;
    })
    .actuatorStatusProvider([](const std::string& reference) -> wolkabout::ActuatorStatus {
        if (reference == "ACTUATOR_REFERENCE_ONE") {
            return wolkabout::ActuatorStatus("65", wolkabout::ActuatorStatus::State::READY);
        } else if (reference == "ACTUATOR_REFERENCE_TWO") {
            return wolkabout::ActuatorStatus("false", wolkabout::ActuatorStatus::State::READY);
        }

        return wolkabout::ActuatorStatus("", wolkabout::ActuatorStatus::State::READY);
    })
    .withPersistence(std::make_shared<CustomPersistenceImpl>()) // Enable data persistance via custom persistence mechanism
    .build();

    wolk->connect();
```

For more info on persistence mechanism see wolkabout::Persistence and wolkabout::InMemoryPersistence classes

**Firmware Update:**

WolkAbout c++ Connector provides mechanism for updating device firmware.

By default this feature is disabled.
See code snippet below on how to enable device firmware update.

```c++

wolkabout::Device device("DEVICE_KEY", "DEVICE_PASSWORD", {"ACTUATOR_REFERENCE_ONE", "ACTUATOR_REFERENCE_TWO"});

class CustomFirmwareInstaller: public wolkabout::FirmwareInstaller
{
public:
	bool install(const std::string& firmwareFile) override
	{
		// Mock install
		std::cout << "Updating firmware with file " << firmwareFile << std::endl;

		// Optionally delete 'firmwareFile
		return true;
	}
};

auto installer = std::make_shared<CustomFirmwareInstaller>();

std::unique_ptr<wolkabout::Wolk> wolk =
  wolkabout::Wolk::newBuilder(device)
    .actuationHandler([](const std::string& reference, const std::string& value) -> void {
        std::cout << "Actuation request received - Reference: " << reference << " value: " << value << std::endl;
    })
    .actuatorStatusProvider([](const std::string& reference) -> wolkabout::ActuatorStatus {
        if (reference == "ACTUATOR_REFERENCE_ONE") {
            return wolkabout::ActuatorStatus("65", wolkabout::ActuatorStatus::State::READY);
        } else if (reference == "ACTUATOR_REFERENCE_TWO") {
            return wolkabout::ActuatorStatus("false", wolkabout::ActuatorStatus::State::READY);
        }

        return wolkabout::ActuatorStatus("", wolkabout::ActuatorStatus::State::READY);
    })
    .withPersistence(std::make_shared<CustomPersistenceImpl>()) // Enable data persistance via custom persistence mechanism
	// Enable firmware update
	.withFirmwareUpdate("2.1.0",								// Current firmware version
						installer,								// Implementation of FirmwareInstaller, which performs installation of obtained device firmware
						".",									// Directory where downloaded device firmware files will be stored
						100 * 1024 * 1024,						// Maximum acceptable size of firmware file, in bytes
						1024 * 1024,							// Size of firmware file transfer chunk, in bytes
						urlDownloader)							// Optional implementation of UrlFileDownloader for cases when one wants to download device firmware via given URL
    .build();

    wolk->connect();
```

**Ping Mechanism:**

WolkAbout C++ Connector by default uses ping mechanism to notify Wolkabout IOT Platform that device is still connected.
Ping message is sent to Wolkabout IOT Platform every 10 minutes.

To reduce network usage ping mechanism can be disabled in following manner:

```cpp
wolkabout::Device device("DEVICE_KEY", "DEVICE_PASSWORD", {"ACTUATOR_REFERENCE_ONE", "ACTUATOR_REFERENCE_TWO"});

std::unique_ptr<wolkabout::Wolk> wolk =
  wolkabout::Wolk::newBuilder(device)
    // actuation handlers setup
    .withoutInternalPing() // Disable ping mechanism
    .build();

    wolk->connect();
```

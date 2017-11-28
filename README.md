# WolkConnector C++
WolkAbout C++ Connector library for connecting devices to WolkAbout IoT platform.

WolkAbout C++ Connector library supports following protocol(s):
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

Sensor readings, actuator statuses, and events are automatically pushed to WolkAbout IoT platform every 50 milliseconds,
hence no action other than *addSensorReading*, *publishActuatorStatus*, or *addEvent* is required.

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

<!---
  Copyright 2021,2022 SECO Mind Srl

  SPDX-License-Identifier: Apache-2.0
-->

# Edgehog ESP32 Device

Edgehog ESP32 Device is the Edgehog component for Espressif
[esp-idf](https://docs.espressif.com/projects/esp-idf/en/latest/), that enables remote management of
Espressif ESP32 devices using [Astarte](https://github.com/astarte-platform/).

## Getting Started

### 1 - Install ESP-IDF Toolchain

Follow the
[esp-idf installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#installation-step-by-step).

### 2 - Install the IDF-Component Manager

Follow the
[idf-component manager installation guide](https://docs.espressif.com/projects/idf-component-manager/en/latest/).

### 3 - Create a New Project from the edgehog_app example

```bash
idf.py create-project-from-example "edgehog-device-manager/edgehog-esp32-device^0.8.0:edgehog_app"
```

### 4 - Build project

```bash
idf.py build
```

## Component Free RTOS APIs usage

The Edgehog ESP32 Device interacts internally with the Free RTOS APIs. As such some factors
should be taken into account when integrating this component in a larger system.

The following **tasks** are spawned directly by the Edgehog ESP32 Device:
- `BLINK TASK`: Provides functionality for visual feedback using an on board LED.
It is only spawned if `CONFIG_INDICATOR_GPIO_ENABLE` is set in the Edgehog ESP32 device
configuration, will use `2048` words from the stack, and can be triggered by a publish from the
Astarte cluster to the dedicated LED interface. This task has a fixed duration and will be deleted
at timeout.
- `OTA UPDATE TASK`: Provides functionality for OTA updates.
It will use `4096` words from the stack, and can be triggered by a publish from the
Astarte cluster to the dedicated OTA update interface. This task does not have a fixed duration, it
will run untill a successful OTA update has been downloaded and flashed or the procedure failed.
Note that the OTA update task could restart the device.

All of the tasks are spawned with the lowest priority and rely on the time slicing functionality
of freertos to run concurrently with the main task.

Other than tasks, a certain number of **software timers** are also created to be used for telemetry.
The actual number of timers that this component is going to use will depend on the
telemetry configuration of your project. Each telemetry type will create a separate timer.
Software timers run all in a single task instantiated by freertos. The stack size and priority for
such task should be configured in the project using this component.
This module has been tested using `2048` words for the stack size and a priority of `1` for the
timer task.

In addition to what stated above, this component requires an Astarte ESP32 Device to be externally
instantiated and provided in its configuration struct. The Astarte ESP32 Device interacts internally
with the Free RTOS APIs and its resource usage should be evaluated separately.

## Resources

* [ESP32 Component Documentation](https://edgehog-device-manager.github.io/docs/snapshot/device-sdks/esp32/)
* [Edgehog documentation](https://edgehog-device-manager.github.io/docs/snapshot/)
* Check out the examples on the right pane of the
[edgehog-esp32-device](https://components.espressif.com/components/edgehog-device-manager/edgehog-esp32-device)
component page or on the
[GitHub repository](https://github.com/edgehog-device-manager/edgehog-esp32-device/tree/main/examples/edgehog_app)

## Contributing

Edgehog ESP32 Device is open to any contribution.

[Pull requests](https://github.com/edgehog-device-manager/edgehog-esp32-device/pulls),
[bug reports and feature requests](https://github.com/edgehog-device-manager/edgehog-esp32-device/issues)
are welcome.

## License

Edgehog ESP32 Device source code is released under the Apache 2.0 License.

Check the [LICENSE](LICENSE) file for more information.

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
[idf-component manager installation guide](https://github.com/espressif/idf-component-manager#installing-the-idf-component-manager).

### 3 - Setup a New Project

Create a new CMake project:

```bash
mkdir ~/hello_world
cd ~/hello_world
cp -r $IDF_PATH/examples/get-started/hello_world .
```

### 4 - Setup Dependencies

Create a new `idf_component.yml` in the project root directory, as follows:

```yaml
## IDF Component Manager Manifest File
dependencies:
  idf:
    version: ">=4.1.0"
  edgehog-esp32-device:
    version: "*" # this is the latest commit on the main branch
    git: https://github.com/edgehog-device-manager/edgehog-esp32-device.git
  astarte-device-sdk-esp32:
    version: "b47e453777f774835b3f86eeb0ce5d3b3a890c9c" # 'release-1.0' branch
    git: https://github.com/astarte-platform/astarte-device-sdk-esp32.git

```

### 5 - Build project

```bash
idf.py build
```

## Contributing

Edgehog ESP32 Device is open to any contribution.

[pull requests](https://github.com/edgehog-device-manager/edgehog-esp32-device/pulls),
[bug reports and feature requests](https://github.com/edgehog-device-manager/edgehog-esp32-device/issues)
are welcome.

## License

Edgehog ESP32 Device source code is released under the Apache 2.0 License.

Check the [LICENSE](LICENSE) file for more information.

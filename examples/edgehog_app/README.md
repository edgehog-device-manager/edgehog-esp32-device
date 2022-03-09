# Edgehog app example

## How to use example

### Setup project using idf component manager (recommended)

* [Install the idf-component manager](https://github.com/espressif/idf-component-manager#installing-the-idf-component-manager)
* run
  ```bash
  idf.py reconfigure
  ```

### Setup project using git

* Add Astarte component

    ``` bash
    git clone https://github.com/astarte-platform/astarte-device-sdk-esp32.git -b release-1.0 ./components/astarte-device-sdk-esp32
    ```
### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

In the `Edgehog example` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.

In the `Component config -> Astarte SDK` submenu:

* Set the Astarte configuration.
    * Set `realm name`.
    * Set `Astarte Pairing base URL`
    * Set `Pairing JWT` you've generated before.

If you have deployed Astarte through docker-compose, the Astarte Pairing base URL is http://<your-machine-url>:4003. On
a common, standard installation, the base URL can be built by adding `/pairing` to your API base URL, e.g.
`https://api.astarte.example.com/pairing`.

## OTA
The OTA update mechanism allows a device to update itself. It is based on
`esp_https_ota` that provides simplified APIs to perform firmware upgrades
over HTTPS.

OTA requires configuring the Partition Table of the device with at least
two `OTA app slot` partitions (i.e. ota_0 and ota_1) and an `OTA Data Partition`.

The simplest way to use the partition table with OTA is to open the project configuration
menu (idf.py menuconfig) and choose partition tables under CONFIG_PARTITION_TABLE_TYPE selecting
`Factory app, two OTA definitions` or create a custom Partition Table.

The OTA operation functions write a new app firmware image to whichever OTA app
slot that is currently not selected for booting. Once the image is verified, the
OTA Data partition is updated to specify that this image should be used for the next boot.

[see EPS OTA](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.


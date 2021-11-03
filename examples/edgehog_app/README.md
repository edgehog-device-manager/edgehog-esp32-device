# Edgehog app example

## How to use example

### Setup project

* Add Astarte component

    ``` bash
    git clone https://github.com/astarte-platform/astarte-device-sdk-esp32.git ./examples/edgehog_app/components/astarte-device-sdk-esp32
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

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.


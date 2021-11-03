#ifndef EDGEHOG_ESP32_DEVICE_ASTARTE_HANDLER_H
#define EDGEHOG_ESP32_DEVICE_ASTARTE_HANDLER_H

#include "astarte_device.h"
#include <esp_err.h>
#include "data/devman_data.h"

astarte_device_handle_t astarte_handler_init();
char *astarte_handler_get_hardware_id_encoded();
esp_err_t add_interfaces(astarte_device_handle_t device);
esp_err_t publish_device_hardware_info(astarte_device_handle_t device, const HardwareInfo* hardwareInfo);

#endif // EDGEHOG_ESP32_DEVICE_ASTARTE_HANDLER_H

//
// Created by harlem88 on 03/11/21.
//

#ifndef EDGEHOG_ESP32_DEVICE_ASTARTE_HANDLER_H
#define EDGEHOG_ESP32_DEVICE_ASTARTE_HANDLER_H

#include "astarte_device.h"
#include <esp_err.h>

bool astarteHandler_start(astarte_device_handle_t astarte_device_handle);
bool astarteHandler_stop(astarte_device_handle_t astarte_device_handle);

astarte_device_handle_t astarteHandler_init();
char *astarte_handler_get_hardware_id_encoded();
esp_err_t add_interfaces(astarte_device_handle_t device);

#endif // EDGEHOG_ESP32_DEVICE_ASTARTE_HANDLER_H

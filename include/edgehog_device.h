/*
 * This file is part of Edgehog.
 *
 * Copyright 2021 SECO Mind Srl
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EDGEHOG_DEVICE_H
#define EDGEHOG_DEVICE_H

typedef struct edgehog_device_t *edgehog_device_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

#include "edgehog.h"
#include <astarte_device.h>
#include <esp_err.h>
#include <nvs.h>

/**
 * @brief Edgehog device configuration struct
 *
 * @details This struct is used to collect all the data needed by the edgehog_device_new function.
 * Pay attention that astarte_device is required and must not be null, while partition_label is
 * completely optional. If no partition label is provided, NVS_DEFAULT_PART_NAME will be used.
 * The values provided with this struct are not copied, do not free() them before calling
 * edgehog_device_destroy.
 */
typedef struct
{
    astarte_device_handle_t astarte_device;
    const char *partition_label;
} edgehog_device_config_t;

/**
 * @brief create Edgehog device handle.
 *
 * @details This function creates an Edgehog device handle. It must be called before anything else.
 *
 * Example:
 *  astarte_device_handle_t astarte_device = astarte_device_init();
 *
 *  edgehog_device_config_t edgehog_conf = {
 *      .astarte_device = astarte_device,
 *  };
 *
 *  edgehog_device_handle_t edgehog_device = edgehog_device_new(&edgehog_conf);
 *
 * @param config An edgehog_device_config_t struct.
 * @return The handle to the device, NULL if an error occurred.
 */

edgehog_device_handle_t edgehog_device_new(edgehog_device_config_t *config);

/**
 * @brief destroy Edgehog device.
 *
 * @details This function destroys the device, freeing all its resources.
 * @param edgehog_device A valid Edgehog device handle.
 */
void edgehog_device_destroy(edgehog_device_handle_t edgehog_device);

/**
 * @brief set the appliance serial number
 *
 * @details This function sends the appliance serial number on Astarte and stores it on the nvs.
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param serial_num The serial number to be stored
 * @return ESP_OK if the data was successfully stored and sent, an esp_err_t otherwise.
 */
esp_err_t edgehog_device_set_appliance_serial_number(
    edgehog_device_handle_t edgehog_device, const char *serial_num);

/**
 * @brief set the appliance part number
 *
 * @details This function sends the appliance part number on Astarte and stores it on the nvs.
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param part_num The part number to be stored
 * @return ESP_OK if the data was successfully stored and sent, an esp_err_t otherwise.
 */
esp_err_t edgehog_device_set_appliance_part_number(
    edgehog_device_handle_t edgehog_device, const char *part_num);

/**
 * @brief receive data from Astarte Server.
 *
 * @details This function must be called when an Astarte Data event coming from server.
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param event A valid Astarte device data event.
 */
void edgehog_device_astarte_event_handler(
    edgehog_device_handle_t edgehog_device, astarte_device_data_event_t *event);

typedef enum
{
    EH_TM_ALL, /**< telemetry: all */
    EH_TM_SYSTEM, /**< telemetry: system */
    EH_TM_LOCATION, /**< telemetry: location */
} edgehog_device_telemetry_type_t;

/**
 * @brief publish Edgehog telemetry.
 *
 * @details This function sends the Edgehog telemetry,
 * according to the selected type;
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param type Edgehog telemetry type.
 */
void edgehog_device_publish_telemetry(
    edgehog_device_handle_t edgehog_device, edgehog_device_telemetry_type_t type);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_DEVICE_H

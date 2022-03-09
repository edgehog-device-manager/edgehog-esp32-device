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

#ifndef EDGEHOG_DEVICE_PRIVATE_H
#define EDGEHOG_DEVICE_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "edgehog_device.h"
#include "edgehog_telemetry.h"
#if CONFIG_INDICATOR_GPIO_ENABLE
#include "edgehog_led.h"
#endif

#include <astarte_list.h>

struct edgehog_device_t
{
    char boot_id[ASTARTE_UUID_LEN];
    astarte_device_handle_t astarte_device;
    const char *partition_name;
#if CONFIG_INDICATOR_GPIO_ENABLE
    edgehog_led_behavior_manager_handle_t led_manager;
#endif
    edgehog_telemetry_t *edgehog_telemetry;

    struct astarte_list_head_t battery_list;
    struct astarte_list_head_t geolocation_list;
};

/**
 * @brief open the Edgehog non-volatile storage with a given namespace.
 *
 * @details This function open an Edgehog non-volatile storage with a given namespace
 * from specified partition defined in the edgehog_device_config_t.
 *
 * @param[in] edgehog_device A valid Edgehog device handle.
 * @param[in] name Namespace name.
 * @param[out] out_handle If successful (return code is zero), handle will be
 *                        returned in this argument.
 * @return ESP_OK if storage handle was opened successfully, an esp_err_t otherwise.
 */
esp_err_t edgehog_device_nvs_open(
    edgehog_device_handle_t edgehog_device, char *name, nvs_handle_t *out_handle);

/**
 * @brief create an iterator to enumerate NVS entries based on one or more parameters.
 *
 * @details This function create an in iterator from specified partition defined in the
 * edgehog_device_config_t to enumerate NVS entries based on one or more parameters
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param namespace Namespace name.
 * @param type One of nvs_type_t values.
 *
 * @return Iterator used to enumerate all the entries found,or NULL if no entry satisfying criteria
 * was found. Iterator obtained through this function has to be released using nvs_release_iterator
 * when not used any more.
 */
nvs_iterator_t edgehog_device_nvs_entry_find(
    edgehog_device_handle_t edgehog_device, const char *namespace, nvs_type_t type);

/**
 * @brief Telemetry periodic callback type.
 */
typedef void (*telemetry_periodic)(edgehog_device_handle_t edgehog_device);

/**
 * @brief get a telemetry periodic callback.
 *
 * @details This function returns a telemetry periodic based on telemetry_type parameter.
 *
 * @param type One of telemetry_type_t values.
 *
 * @return a telemetry periodic,or NULL if no periodic satisfying criteria
 * was found.
 */
telemetry_periodic edgehog_device_get_telemetry_periodic(telemetry_type_t type);

/**
 * @brief get a telemetry type.
 *
 * @details This function returns a telemetry type based on interface_name parameter.
 *
 * @param interface_name The interface name.
 *
 * @return a telemetry type,or EDGEHOG_TL_INVALID if no type satisfying criteria
 * was found.
 */
telemetry_type_t edgehog_device_get_telemetry_type(const char *interface_name);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_DEVICE_PRIVATE_H

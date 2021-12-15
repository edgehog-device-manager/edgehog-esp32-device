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

#ifndef EDGEHOG_TELEMETRY_H
#define EDGEHOG_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "edgehog_device.h"

extern const astarte_interface_t telemetry_config_interface;
typedef struct edgehog_telemetry_data edgehog_telemetry_t;

/**
 * @brief create Edgehog telemetry.
 *
 * @details This function creates an Edgehog telemetry.
 *
 * @param telemetry_config An edgehog_device_telemetry_config_t struct.
 * @param telemetry_config_len Number of telemetry config elements.
 * @return A pointer to Edgehog telemetry or a NULL if an error occurred.
 */
edgehog_telemetry_t *edgehog_telemetry_new(
    edgehog_device_telemetry_config_t *telemetry_config, size_t telemetry_config_len);

/**
 * @brief Start Edgehog telemetry.
 *
 * @details This function starts an Edgehog telemetry.
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param edgehog_telemetry A valid Edgehog telemetry pointer.
 */
edgehog_err_t edgehog_telemetry_start(
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry);

/**
 * @brief receive Edgehog telemetry config.
 *
 * @details This function receives a telemetry config request from Astarte.
 *
 * @param event_request A valid Astarte device data event.
 *
 * @return EDGEHOG_OK if the config event is handled successfully, an edgehog_err_t otherwise.
 */

edgehog_err_t edgehog_telemetry_config_event(astarte_device_data_event_t *event_request,
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry);

/**
 * @brief destroy Edgehog telemetry.
 *
 * @details This function destroys the telemetry, freeing all its resources.
 * @param edgehog_telemetry A valid Edgehog telemetry pointer.
 */
void edgehog_telemetry_destroy(edgehog_telemetry_t *edgehog_telemetry);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_TELEMETRY_H

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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EDGEHOG_OTA_H
#define EDGEHOG_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "edgehog_device.h"

extern const astarte_interface_t ota_request_interface;
extern const astarte_interface_t ota_event_interface;

/**
 * @brief initialize Edgehog device OTA.
 *
 * @details This function initializes the OTA procedure and
 * if there is any pending OTA it completes it.
 *
 * @param edgehog_dev A valid Edgehog device handle.
 */
void edgehog_ota_init(edgehog_device_handle_t edgehog_dev);

/**
 * @brief receive Edgehog device OTA.
 *
 * @details This function receives an OTA event request from Astarte. This function may spawn a
 * new task to preform the OTA update.
 *
 * @param edgehog_dev A valid Edgehog device handle.
 * @param event_request A valid Astarte device data event.
 *
 * @return EDGEHOG_OK if the OTA event is handled successfully, an edgehog_err_t otherwise.
 */
edgehog_err_t edgehog_ota_event(
    edgehog_device_handle_t edgehog_dev, astarte_device_data_event_t *event_request);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_OTA_H

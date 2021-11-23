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

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_DEVICE_PRIVATE_H

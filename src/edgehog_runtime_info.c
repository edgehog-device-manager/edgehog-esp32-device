/*
 * This file is part of Edgehog.
 *
 * Copyright 2022 SECO Mind Srl
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

#include "edgehog_runtime_info.h"
#include "edgehog_device_private.h"
#include <esp_idf_version.h>
#include <esp_log.h>
#include <stdio.h>
#include <string.h>

#define RUNTIME_NAME "edgehog-esp32-device"
#define RUNTIME_URL "https://github.com/edgehog-device-manager/edgehog-esp32-device"
#define RUNTIME_VERSION "0.8.1"

static const char *TAG = "EDGEHOG_RUNTIME_INFO";

const astarte_interface_t runtime_info_interface = { .name = "io.edgehog.devicemanager.RuntimeInfo",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_PROPERTIES };

void edgehog_runtime_info_publish(edgehog_device_handle_t edgehog_device)
{
    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;
    astarte_err_t ret = astarte_device_set_string_property(
        astarte_device, runtime_info_interface.name, "/name", RUNTIME_NAME);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish runtime name");
        return;
    }
    ret = astarte_device_set_string_property(
        astarte_device, runtime_info_interface.name, "/url", RUNTIME_URL);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish runtime URL");
        return;
    }
    ret = astarte_device_set_string_property(
        astarte_device, runtime_info_interface.name, "/version", RUNTIME_VERSION);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish runtime version");
        return;
    }

    const char *idf_version = esp_get_idf_version();
    size_t env_size = strlen(idf_version) + strlen("esp-idf ") + 1;
    char *environment = malloc(env_size);
    if (!environment) {
        ESP_LOGE(TAG, "Unable to allocate memory for environment");
        return;
    }
    snprintf(environment, env_size, "esp-idf %s", idf_version);
    ret = astarte_device_set_string_property(
        astarte_device, runtime_info_interface.name, "/environment", environment);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish runtime environment");
    }
    free(environment);
}

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

#include "edgehog_base_image.h"
#include "edgehog_device_private.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#ifdef BUILD_DATE_TIME

#define BUILD_ID BUILD_DATE_TIME
#else
#define BUILD_ID ""
#endif

static const char *TAG = "EDGEHOG_BASE_IMAGE";

const astarte_interface_t base_image_interface = { .name = "io.edgehog.devicemanager.BaseImage",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_PROPERTIES };

void edgehog_base_image_data_publish(edgehog_device_handle_t edgehog_device)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    const esp_app_desc_t *desc = esp_app_get_description();
#else
    const esp_app_desc_t *desc = esp_ota_get_app_description();
#endif
    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;

    astarte_err_t ret = astarte_device_set_string_property(
        astarte_device, base_image_interface.name, "/name", desc->project_name);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish os image name");
        return;
    }

    ret = astarte_device_set_string_property(
        astarte_device, base_image_interface.name, "/version", desc->version);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish os image version");
        return;
    }

    ret = astarte_device_set_string_property(
        astarte_device, base_image_interface.name, "/buildId", BUILD_ID);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish os image build id");
        return;
    }

    char sha256_str[65];
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_app_get_elf_sha256(sha256_str, 65);
#else
    esp_ota_get_app_elf_sha256(sha256_str, 65);
#endif
    ret = astarte_device_set_string_property(
        astarte_device, base_image_interface.name, "/fingerprint", sha256_str);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish os image hash");
    }
}

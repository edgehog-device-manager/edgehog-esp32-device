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

#include "edgehog_os_info.h"
#include "edgehog_device_private.h"
#include <esp_idf_version.h>
#include <esp_log.h>

#define TAG "EDGEHOG_OS_INFO"

const astarte_interface_t os_info_interface = { .name = "io.edgehog.devicemanager.OSInfo",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_PROPERTIES };

void edgehog_device_publish_os_info(edgehog_device_handle_t edgehog_device)
{
    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;
    esp_err_t ret = astarte_device_set_string_property(
        astarte_device, os_info_interface.name, "/osName", "esp-idf");
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to set osName property");
        return;
    }
    ret = astarte_device_set_string_property(
        astarte_device, os_info_interface.name, "/osVersion", esp_get_idf_version());
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to set osVersion property");
    }
}

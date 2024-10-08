/*
 * This file is part of Edgehog.
 *
 * Copyright 2021,2022 SECO Mind Srl
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

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>

#include "example_task.h"
#include "wifi.h"

/************************************************
 * Constants/Defines
 ***********************************************/

static const char *TAG = "EXAMPLE_MAIN";

/************************************************
 * Main function definition
 ***********************************************/

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(nvs_flash_init_partition(ASTARTE_PARTITION_NAME));
    ESP_ERROR_CHECK(nvs_flash_init_partition(EDGEHOG_PARTITION_NAME));

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    ESP_LOGI(TAG, "NVS and WIFI initialization completed.");

    xTaskCreate(edgehog_example_task, "edgehog_example_task", 6000, NULL, tskIDLE_PRIORITY, NULL);
}

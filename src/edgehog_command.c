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

#include "edgehog_command.h"
#include "astarte_bson.h"
#include <astarte_bson_types.h>
#include <esp_event.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "EDGEHOG_COMMANDS";

const astarte_interface_t commands_interface = { .name = "io.edgehog.devicemanager.Commands",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_SERVER,
    .type = TYPE_DATASTREAM };

edgehog_err_t edgehog_command_event(astarte_device_data_event_t *event_request)
{
    EDGEHOG_VALIDATE_INCOMING_DATA(TAG, event_request, "/request", BSON_TYPE_STRING);

    uint32_t len;
    const char *command = astarte_bson_value_to_string(event_request->bson_element.value, &len);

    if (strcmp(command, "Reboot") == 0) {
        ESP_LOGI(TAG, "Device will restart in 1 second");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGW(TAG, "Unable to handle command event, command %s unsupported", command);
        return EDGEHOG_ERR;
    }
}

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

#include "edgehog_battery_status.h"
#include <astarte_bson_serializer.h>
#include <esp_log.h>
#include <stdio.h>

static const char *TAG = "EDGEHOG_BATTERY";

const astarte_interface_t battery_status_interface
    = { .name = "io.edgehog.devicemanager.BatteryStatus",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

static const char *edgehog_battery_to_code(edgehog_battery_state state);
static double normalize_error_level(double level);

void edgehog_battery_status_publish(astarte_device_handle_t astarte_device,
    const char *battery_slot, double level_percentage, double level_absolute_error,
    edgehog_battery_state battery_state)
{
    const char *battery_status = edgehog_battery_to_code(battery_state);
    struct astarte_bson_serializer_t bs;
    astarte_bson_serializer_init(&bs);
    astarte_bson_serializer_append_double(
        &bs, "levelPercentage", normalize_error_level(level_percentage));
    astarte_bson_serializer_append_double(
        &bs, "levelAbsoluteError", normalize_error_level(level_absolute_error));
    astarte_bson_serializer_append_string(&bs, "status", battery_status);
    astarte_bson_serializer_append_end_of_document(&bs);

    size_t path_size = strlen(battery_slot) + 2;
    char *path = malloc(path_size);
    if (!path) {
        ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }
    snprintf(path, path_size, "/%s", battery_slot);

    int doc_len;
    const void *doc = astarte_bson_serializer_get_document(&bs, &doc_len);
    astarte_device_stream_aggregate(astarte_device, battery_status_interface.name, path, doc, 0);
    astarte_bson_serializer_destroy(&bs);
    free(path);
}

static const char *edgehog_battery_to_code(edgehog_battery_state state)
{
    switch (state) {
        case BATTERY_IDLE:
            return "Idle";
        case BATTERY_CHARGING:
            return "Charging";
        case BATTERY_DISCHARGING:
            return "Discharging";
        case BATTERY_IDLE_OR_CHARGING:
            return "EitherIdleOrCharging";
        case BATTERY_FAILURE:
            return "Failure";
        case BATTERY_REMOVED:
            return "Removed";
        case BATTERY_UNKNOWN:
            return "Unknown";
        default:
            return "";
    }
}

static double normalize_error_level(double level)
{
    if (level > 100) {
        return 100;
    } else if (level < 0) {
        return 0;
    }
    return level;
}

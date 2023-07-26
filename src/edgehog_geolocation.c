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

#include "edgehog_geolocation.h"
#include "astarte_bson_serializer.h"
#include "edgehog_device_private.h"
#include "edgehog_geolocation_p.h"
#include <esp_log.h>

static const char *TAG = "EDGEHOG_GEOLOCATION";

const astarte_interface_t geolocation_interface = { .name = "io.edgehog.devicemanager.Geolocation",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_DATASTREAM };

struct geolocation_info_t
{
    astarte_list_head_t head;
    bool updated;
    char *id;
    double longitude;
    double latitude;
    double accuracy;
    double altitude;
    double altitude_accuracy;
    double heading;
    double speed;
};

void edgehog_geolocation_delete_list(astarte_list_head_t *geolocation_list)
{
    astarte_list_head_t *item;
    astarte_list_head_t *tmp;
    MUTABLE_LIST_FOR_EACH(item, tmp, geolocation_list)
    {
        struct geolocation_info_t *status = GET_LIST_ENTRY(item, struct geolocation_info_t, head);
        free(status->id);
        free(status);
    }
}

static struct geolocation_info_t *find_geolocation(
    astarte_list_head_t *geolocation_list, const char *gps_id)
{
    astarte_list_head_t *item;
    LIST_FOR_EACH(item, geolocation_list)
    {
        struct geolocation_info_t *status = GET_LIST_ENTRY(item, struct geolocation_info_t, head);
        if (strcmp(status->id, gps_id) == 0) {
            return status;
        }
    }

    return NULL;
}

static struct geolocation_info_t *get_geolocation_info(
    edgehog_device_handle_t edgehog_device, const char *gps_id)
{
    struct geolocation_info_t *status = find_geolocation(&edgehog_device->geolocation_list, gps_id);
    if (!status) {
        status = calloc(1, sizeof(struct geolocation_info_t));
        if (!status) {
            return NULL;
        }

        status->updated = false;
        status->id = strdup(gps_id);
        status->longitude = 0;
        status->latitude = 0;
        status->accuracy = 0;
        status->altitude = 0;
        status->altitude_accuracy = 0;
        status->heading = 0;
        status->speed = 0;
        if (!status->id) {
            free(status);
            return NULL;
        }

        astarte_list_append(&edgehog_device->geolocation_list, &status->head);
    }

    return status;
}

void edgehog_geolocation_update(
    edgehog_device_handle_t edgehog_device, edgehog_geolocation_data_t *update)
{
    struct geolocation_info_t *status = get_geolocation_info(edgehog_device, update->id);
    if (!status) {
        ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }

    if (!status->updated
        && (status->longitude != update->longitude || status->latitude != update->latitude
            || status->accuracy != update->accuracy || status->altitude != update->altitude
            || status->altitude_accuracy != update->altitude_accuracy
            || status->heading != update->heading || status->speed != update->speed)) {
        status->updated = true;
        status->longitude = update->longitude;
        status->latitude = update->latitude;
        status->accuracy = update->accuracy;
        status->altitude = update->altitude;
        status->altitude_accuracy = update->altitude_accuracy;
        status->heading = update->heading;
        status->speed = update->speed;
    }
}

void edgehog_geolocation_publish(edgehog_device_handle_t edgehog_device)
{
    astarte_list_head_t *item;
    LIST_FOR_EACH(item, &edgehog_device->geolocation_list)
    {
        struct geolocation_info_t *data = GET_LIST_ENTRY(item, struct geolocation_info_t, head);

        if (!data->updated) {
            continue;
        }
        astarte_bson_serializer_handle_t bs = astarte_bson_serializer_new();
        astarte_bson_serializer_append_double(bs, "latitude", data->latitude);
        astarte_bson_serializer_append_double(bs, "longitude", data->longitude);
        astarte_bson_serializer_append_double(bs, "accuracy", data->accuracy);
        astarte_bson_serializer_append_double(bs, "altitude", data->altitude);
        astarte_bson_serializer_append_double(bs, "altitudeAccuracy", data->altitude_accuracy);
        astarte_bson_serializer_append_double(bs, "heading", data->heading);
        astarte_bson_serializer_append_double(bs, "speed", data->speed);
        astarte_bson_serializer_append_end_of_document(bs);

        size_t path_size = strlen(data->id) + 2;
        char *path = malloc(path_size);
        if (!path) {
            ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
            return;
        }
        snprintf(path, path_size, "/%s", data->id);

        const void *doc = astarte_bson_serializer_get_document(bs, NULL);
        astarte_err_t res = astarte_device_stream_aggregate(
            edgehog_device->astarte_device, geolocation_interface.name, path, doc, 0);

        astarte_bson_serializer_destroy(bs);
        free(path);

        if (res == ASTARTE_OK) {
            data->updated = false;
        }
    }
}

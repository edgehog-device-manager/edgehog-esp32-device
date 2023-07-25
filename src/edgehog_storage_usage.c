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

#include "edgehog_storage_usage.h"
#include "edgehog_device_private.h"
#include <astarte_bson_serializer.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <nvs.h>

// A key-value pair might span multiple entries, each entry is 32 bytes
#define NVS_ENTRY_SIZE_BYTES 32

static const char *TAG = "EDGEHOG_STORAGE";

const astarte_interface_t storage_usage_interface
    = { .name = "io.edgehog.devicemanager.StorageUsage",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

static void publish_storage_usage(
    astarte_device_handle_t astarte_device, const char *label, long free, long total);

void edgehog_storage_usage_publish(edgehog_device_handle_t edgehog_device)
{
    esp_partition_iterator_t partition_iterator
        = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);

    while (partition_iterator) {
        const esp_partition_t *partition_info = esp_partition_get(partition_iterator);
        if (partition_info) {
            nvs_stats_t nvs_stats;
            esp_err_t result = nvs_get_stats(partition_info->label, &nvs_stats);
            if (result == ESP_OK) {
                publish_storage_usage(edgehog_device->astarte_device, partition_info->label,
                    nvs_stats.free_entries * NVS_ENTRY_SIZE_BYTES,
                    nvs_stats.total_entries * NVS_ENTRY_SIZE_BYTES);
            }
        }
        partition_iterator = esp_partition_next(partition_iterator);
    }
}

static void publish_storage_usage(
    astarte_device_handle_t astarte_device, const char *label, long free_bytes, long total_bytes)
{
    astarte_bson_serializer_handle_t bs = astarte_bson_serializer_new();
    astarte_bson_serializer_append_int64(bs, "freeBytes", free_bytes);
    astarte_bson_serializer_append_int64(bs, "totalBytes", total_bytes);
    astarte_bson_serializer_append_end_of_document(bs);

    size_t path_size = strlen(label) + 2;
    char *path = malloc(path_size);
    if (!path) {
        ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
        return;
    }
    snprintf(path, path_size, "/%s", label);

    const void *doc = astarte_bson_serializer_get_document(bs, NULL);
    astarte_device_stream_aggregate(astarte_device, storage_usage_interface.name, path, doc, 0);
    astarte_bson_serializer_destroy(bs);
    free(path);
}

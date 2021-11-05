/*
 * This file is part of Edgehog.
 *
 * Copyright 2021 SECO Mind
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

#include "edgehog.h"
#include "esp_system.h"
#include <astarte_bson_serializer.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <uuid.h>

static const char *TAG = "EDGEHOG";

struct edgehog_device_t
{
    char boot_id[38];
    astarte_device_handle_t astarte_device;
};

const static astarte_interface_t hardware_info_interface
    = { .name = "io.edgehog.devicemanager.HardwareInfo",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_PROPERTIES };

const static astarte_interface_t system_status_status_interface
    = { .name = "io.edgehog.devicemanager.SystemStatus",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

esp_err_t add_interfaces(astarte_device_handle_t astarte_device);
static void publish_device_hardware_info(astarte_device_handle_t astarte_device);
static void publish_system_status(edgehog_device_handle_t edgehog_device);

edgehog_device_handle_t edgehog_new(astarte_device_handle_t astarte_device)
{
    if (!astarte_device) {
        ESP_LOGE(TAG, "Unable to init Edgehog device, Astarte device was NULL");
        return NULL;
    }

    edgehog_device_handle_t edgehog_device = calloc(1, sizeof(struct edgehog_device_t));
    if (!edgehog_device) {
        ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
        return NULL;
    }

    edgehog_device->astarte_device = astarte_device;
    uuid_t boot_id;
    uuid_generate_v4(boot_id);
    uuid_to_string(boot_id, edgehog_device->boot_id);

    ESP_ERROR_CHECK(add_interfaces(astarte_device));
    publish_device_hardware_info(astarte_device);
    publish_system_status(edgehog_device);
    return edgehog_device;
}

esp_err_t add_interfaces(astarte_device_handle_t device)
{
    astarte_err_t ret;
    ret = astarte_device_add_interface(device, &hardware_info_interface);

    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to add Astarte Interface ( %s ) error code: %d",
            hardware_info_interface.name, ret);
        return ESP_FAIL;
    }

    ret = astarte_device_add_interface(device, &system_status_status_interface);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to add Astarte Interface ( %s ) error code: %d",
            hardware_info_interface.name, ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void publish_device_hardware_info(astarte_device_handle_t astarte_device)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    char *cpu_architecture = "Xtensa";
    char *cpu_vendor = "Espressif Systems";
    char *cpu_model;
    char *cpu_model_name;
    long mem_total_bytes;

    switch (chip_info.model) {
        case CHIP_ESP32:
            cpu_model = "ESP32";
            if (chip_info.cores == 1) {
                cpu_model_name = "Single-core Xtensa LX6";
            } else {
                cpu_model_name = "Dual-core Xtensa LX6";
            }
            break;
        case CHIP_ESP32S2:
            cpu_model = "ESP32-S2";
            cpu_model_name = "Single-core Xtensa LX7";
            break;
        case CHIP_ESP32S3:
            cpu_model = "ESP32-S3";
            cpu_model_name = "Dual-core Xtensa LX7";
            break;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 1)
        case CHIP_ESP32C3:
            cpu_model = "ESP32-C3";
            cpu_model_name = "Single-core 32-bit RISC-V";
            break;
#endif
        default:
            cpu_model = "GENERIC";
            cpu_model_name = "Generic";
    }

    mem_total_bytes = (long) heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
#ifdef CONFIG_SPIRAM_USE
    mem_total_bytes += (long) heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
#endif
    astarte_device_set_string_property(
        astarte_device, hardware_info_interface.name, "/cpu/architecture", cpu_architecture);
    astarte_device_set_string_property(
        astarte_device, hardware_info_interface.name, "/cpu/model", cpu_model);
    astarte_device_set_string_property(
        astarte_device, hardware_info_interface.name, "/cpu/modelName", cpu_model_name);
    astarte_device_set_string_property(
        astarte_device, hardware_info_interface.name, "/cpu/vendor", cpu_vendor);
    astarte_device_set_longinteger_property(
        astarte_device, hardware_info_interface.name, "/mem/totalBytes", mem_total_bytes);
}

static void publish_system_status(edgehog_device_handle_t edgehog_device)
{
    int64_t uptime_millis = esp_timer_get_time() / 1000;
    uint32_t avail_memory = esp_get_free_heap_size();
    int task_count = uxTaskGetNumberOfTasks();

    struct astarte_bson_serializer_t bs;
    astarte_bson_serializer_init(&bs);
    astarte_bson_serializer_append_int64(&bs, "availMemoryBytes", avail_memory);
    astarte_bson_serializer_append_string(&bs, "bootId", edgehog_device->boot_id);
    astarte_bson_serializer_append_int32(&bs, "taskCount", task_count);
    astarte_bson_serializer_append_int64(&bs, "uptimeMillis", uptime_millis);
    astarte_bson_serializer_append_end_of_document(&bs);

    int doc_len;
    const void *doc = astarte_bson_serializer_get_document(&bs, &doc_len);
    astarte_device_stream_aggregate(edgehog_device->astarte_device,
        system_status_status_interface.name, "/systemStatus", doc, 0);
    astarte_bson_serializer_destroy(&bs);
}

void edgehog_device_destroy(edgehog_device_handle_t edgehog_device)
{
    if (edgehog_device) {
        astarte_device_destroy(edgehog_device->astarte_device);
    }

    free(edgehog_device);
}

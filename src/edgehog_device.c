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
#include "edgehog_battery_status.h"
#include "edgehog_battery_status_p.h"
#include "edgehog_cellular_connection.h"
#include "edgehog_command.h"
#include "edgehog_device_private.h"
#include "edgehog_geolocation.h"
#include "edgehog_geolocation_p.h"
#include "edgehog_network_interface.h"
#include "edgehog_os_info.h"
#include "edgehog_ota.h"
#include "edgehog_runtime_info.h"
#include "edgehog_storage_usage.h"
#include "esp_system.h"
#include <astarte_bson_serializer.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <uuid.h>

#define SYSTEM_NAMESPACE "eh_system"

static const char *TAG = "EDGEHOG";

const static astarte_interface_t hardware_info_interface
    = { .name = "io.edgehog.devicemanager.HardwareInfo",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_PROPERTIES };

const static astarte_interface_t wifi_scan_result_interface
    = { .name = "io.edgehog.devicemanager.WiFiScanResults",
          .major_version = 0,
          .minor_version = 2,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

const static astarte_interface_t system_status_status_interface
    = { .name = "io.edgehog.devicemanager.SystemStatus",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

const static astarte_interface_t system_info_interface
    = { .name = "io.edgehog.devicemanager.SystemInfo",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_PROPERTIES };

ESP_EVENT_DEFINE_BASE(EDGEHOG_EVENTS);

static esp_err_t add_interfaces(astarte_device_handle_t astarte_device);
static void publish_device_hardware_info(edgehog_device_handle_t edgehog_device);
static void publish_system_status(edgehog_device_handle_t edgehog_device);
static void publish_wifi_ap(edgehog_device_handle_t edgehog_device);
static void scan_wifi_ap(edgehog_device_handle_t edgehog_device);
static inline bool compare_mac_address(const uint8_t a[], const uint8_t b[]);

static void edgehog_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (!arg || !event_data) {
        return;
    }

    // this if statement could become a double nested switch statement in the future
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        wifi_event_sta_scan_done_t *wifi_event_sta_scan_done
            = (wifi_event_sta_scan_done_t *) event_data;
        edgehog_device_handle_t edgehog_device = (edgehog_device_handle_t) arg;
        if (wifi_event_sta_scan_done->status == 0) {
            // status of scanning APs: 0 — success, 1 - failure
            publish_wifi_ap(edgehog_device);
            esp_event_handler_instance_unregister(
                WIFI_EVENT, WIFI_EVENT_SCAN_DONE, edgehog_event_handler);
        }
    }
}

void edgehog_device_astarte_event_handler(
    edgehog_device_handle_t edgehog_device, astarte_device_data_event_t *event)
{
    if (!edgehog_device) {
        ESP_LOGE(TAG, "Unable to handle event, Edgehog device undefined");
        return;
    }

    if (strcmp(event->interface_name, ota_request_interface.name) == 0) {
        // Beware this function blocks the caller until OTA is completed.
        edgehog_err_t ota_result = edgehog_ota_event(edgehog_device, event);
        if (ota_result == EDGEHOG_OK) {
            ESP_LOGI(TAG, "OTA Deploy end successfully, device restart in 5 seconds");
            vTaskDelay(pdMS_TO_TICKS(5000));
            ESP_LOGI(TAG, "Device restart");
            esp_restart();
        }
    } else if (strcmp(event->interface_name, commands_interface.name) == 0) {
        if (edgehog_command_event(event) != EDGEHOG_OK) {
            ESP_LOGE(TAG, "Unable to handle command request");
        }
    } else if (strcmp(event->interface_name, telemetry_config_interface.name) == 0) {
        edgehog_err_t telemetry_config_result = edgehog_telemetry_config_event(
            event, edgehog_device, edgehog_device->edgehog_telemetry);
        if (telemetry_config_result == EDGEHOG_OK) {
            ESP_LOGI(TAG, "Telemetry config update handled successfully");
        }
    }
#if CONFIG_INDICATOR_GPIO_ENABLE
    if (strcmp(event->interface_name, led_request_interface.name) == 0) {
        ESP_LOGI(TAG, "Incoming request for led behavior");
        edgehog_err_t led_behavior_result
            = edgehog_led_behavior_event(edgehog_device->led_manager, event);
        if (led_behavior_result != EDGEHOG_OK) {
            ESP_LOGE(TAG, "Unable to set led behavior");
        }
    }
#endif
}

edgehog_device_handle_t edgehog_device_new(edgehog_device_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Unable to init Edgehog device, no config provided");
        return NULL;
    }

    if (!config->astarte_device) {
        ESP_LOGE(TAG, "Unable to init Edgehog device, Astarte device was NULL");
        return NULL;
    }

    edgehog_device_handle_t edgehog_device = calloc(1, sizeof(struct edgehog_device_t));
    if (!edgehog_device) {
        ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
        return NULL;
    }

    edgehog_device->astarte_device = config->astarte_device;
    uuid_t boot_id;
    uuid_generate_v4(boot_id);
    uuid_to_string(boot_id, edgehog_device->boot_id);

    if (config->partition_label) {
        edgehog_device->partition_name = config->partition_label;
    } else {
        edgehog_device->partition_name = NVS_DEFAULT_PART_NAME;
    }

    astarte_list_init(&edgehog_device->battery_list);
    astarte_list_init(&edgehog_device->geolocation_list);

    ESP_ERROR_CHECK(add_interfaces(config->astarte_device));
#if CONFIG_INDICATOR_GPIO_ENABLE
    edgehog_device->led_manager = edgehog_led_behavior_manager_new();
#endif
    edgehog_telemetry_t *edgehog_telemetry
        = edgehog_telemetry_new(config->telemetry_config, config->telemetry_config_len);
    if (!edgehog_telemetry) {
        ESP_LOGE(TAG, "Unable to create edgehog telemetry update");
        goto error;
    }
    edgehog_device->edgehog_telemetry = edgehog_telemetry;

    return edgehog_device;

error:
    edgehog_device_destroy(edgehog_device);
    return NULL;
}

static void edgehog_initial_publish(edgehog_device_handle_t edgehog_device)
{
    edgehog_ota_init(edgehog_device);
    publish_device_hardware_info(edgehog_device);
    publish_system_status(edgehog_device);
    edgehog_storage_usage_publish(edgehog_device);
    edgehog_device_publish_os_info(edgehog_device);
    edgehog_base_image_data_publish(edgehog_device);
    edgehog_runtime_info_publish(edgehog_device);
    scan_wifi_ap(edgehog_device);
}

edgehog_err_t edgehog_device_start(edgehog_device_handle_t edgehog_device)
{
    edgehog_initial_publish(edgehog_device);

    edgehog_err_t start_res
        = edgehog_telemetry_start(edgehog_device, edgehog_device->edgehog_telemetry);
    if (start_res != EDGEHOG_OK) {
        ESP_LOGE(TAG, "Unable to start Edgehog device");
    }

    return start_res;
}

esp_err_t add_interfaces(astarte_device_handle_t device)
{
    const astarte_interface_t *const interfaces[]
        = { &hardware_info_interface,
              &system_status_status_interface,
              &wifi_scan_result_interface,
              &system_info_interface,
              &ota_request_interface,
              &ota_response_interface,
              &storage_usage_interface,
              &battery_status_interface,
              &commands_interface,
#if CONFIG_INDICATOR_GPIO_ENABLE
              &led_request_interface,
#endif
              &telemetry_config_interface,
              &os_info_interface,
              &base_image_interface,
              &runtime_info_interface,
              &cellular_connection_properties_interface,
              &cellular_connection_status_interface,
              &netif_interface,
              &geolocation_interface };

    int len = sizeof(interfaces) / sizeof(const astarte_interface_t *);

    for (int i = 0; i < len; i++) {
        astarte_err_t ret = astarte_device_add_interface(device, interfaces[i]);
        if (ret != ASTARTE_OK) {
            ESP_LOGE(TAG, "Unable to add Astarte Interface ( %s ) error code: %d",
                interfaces[i]->name, ret);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static void publish_device_hardware_info(edgehog_device_handle_t edgehog_device)
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
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        case CHIP_ESP32S3:
            cpu_model = "ESP32-S3";
            cpu_model_name = "Dual-core Xtensa LX7";
            break;
#endif
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
    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;
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

static void scan_wifi_ap(edgehog_device_handle_t edgehog_device)
{
    // Register the event at every scan and unregister it at every publish to avoid
    // catching event generated by third party scan
    esp_err_t ret = esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_SCAN_DONE, edgehog_event_handler, edgehog_device, NULL);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
            "Unable to register to default event loop. Be sure to have called "
            "esp_event_loop_create_default() before calling edgehog_device_new");
        return;
    }

    wifi_scan_config_t config = { .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .max = 120 } } };

    esp_wifi_scan_start(&config, false);
}

static void publish_wifi_ap(edgehog_device_handle_t edgehog_device)
{
    uint16_t ap_count = 0;
    esp_err_t ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        return;
    }

    wifi_ap_record_t *ap_info = (wifi_ap_record_t *) malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!ap_info) {
        ESP_LOGE(TAG, "Unable to allocate memory for %d access point records", ap_count);
        return;
    }

    wifi_ap_record_t ap_info_connected;
    bool ap_is_connected = esp_wifi_sta_get_ap_info(&ap_info_connected) == ESP_OK;

    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    if (ret != ESP_OK) {
        free(ap_info);
        return;
    }
    for (int i = 0; i < ap_count; i++) {
        char mac[18];
        snprintf(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x", ap_info[i].bssid[0], ap_info[i].bssid[1],
            ap_info[i].bssid[2], ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);

        struct astarte_bson_serializer_t bs;
        astarte_bson_serializer_init(&bs);
        astarte_bson_serializer_append_int32(&bs, "channel", ap_info[i].primary);
        astarte_bson_serializer_append_string(&bs, "essid", (char *) ap_info[i].ssid);
        astarte_bson_serializer_append_string(&bs, "macAddress", mac);
        astarte_bson_serializer_append_int32(&bs, "rssi", ap_info[i].rssi);
        astarte_bson_serializer_append_boolean(&bs, "connected",
            ap_is_connected && compare_mac_address(ap_info[i].bssid, ap_info_connected.bssid));
        astarte_bson_serializer_append_end_of_document(&bs);

        int doc_len;
        const void *doc = astarte_bson_serializer_get_document(&bs, &doc_len);
        astarte_device_stream_aggregate(
            edgehog_device->astarte_device, wifi_scan_result_interface.name, "/ap", doc, 0);
        astarte_bson_serializer_destroy(&bs);
    }

    free(ap_info);
}

static esp_err_t edgehog_nvs_set_str(const char *partition_name, const char *key, char *value)
{
    nvs_handle nvs;
    esp_err_t ret = nvs_open_from_partition(partition_name, SYSTEM_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to open %s", partition_name);
        return ret;
    }

    ret = nvs_set_str(nvs, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to set %s: %s. Error %d", key, value, ret);
        nvs_close(nvs);
        return ret;
    }

    nvs_commit(nvs);
    nvs_close(nvs);
    return ESP_OK;
}

static char *edgehog_nvs_get_string(const char *partition_name, const char *key)
{
    nvs_handle nvs;
    nvs_open_from_partition(partition_name, SYSTEM_NAMESPACE, NVS_READONLY, &nvs);

    size_t required_size = 0;
    nvs_get_str(nvs, key, NULL, &required_size);
    if (required_size == 0) {
        goto error;
    }
    char *out_value = malloc(required_size * sizeof(char *));
    if (!out_value) {
        goto error;
    }
    nvs_get_str(nvs, key, out_value, &required_size);
    nvs_close(nvs);
    return out_value;

error:
    nvs_close(nvs);
    return NULL;
}

esp_err_t edgehog_device_set_system_serial_number(
    edgehog_device_handle_t edgehog_device, const char *serial_num)
{
    if (!serial_num) {
        return ESP_FAIL;
    }

    char *previous_value = edgehog_nvs_get_string(edgehog_device->partition_name, "serial_number");
    esp_err_t ret;
    if (previous_value && strcmp(previous_value, serial_num) == 0) {
        ret = ESP_OK;
        goto end;
    }

    ret = astarte_device_set_string_property(edgehog_device->astarte_device,
        system_info_interface.name, "/serialNumber", (char *) serial_num);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish serialNumber property. Astarte Error %d", ret);
        ret = ESP_FAIL;
        goto end;
    }
    if (edgehog_device->partition_name) {
        ret = edgehog_nvs_set_str(
            edgehog_device->partition_name, "serial_number", (char *) serial_num);
    } else {
        ret = ESP_OK;
    }

end:
    free(previous_value);
    return ret;
}

esp_err_t edgehog_device_set_system_part_number(
    edgehog_device_handle_t edgehog_device, const char *part_num)
{
    if (!part_num) {
        return ESP_FAIL;
    }

    char *previous_value = edgehog_nvs_get_string(edgehog_device->partition_name, "part_number");
    esp_err_t ret;
    if (previous_value && strcmp(previous_value, part_num) == 0) {
        ret = ESP_OK;
        goto end;
    }

    ret = astarte_device_set_string_property(edgehog_device->astarte_device,
        system_info_interface.name, "/partNumber", (char *) part_num);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish partNumber property. Astarte Error %d", ret);
        ret = ESP_FAIL;
        goto end;
    }
    if (edgehog_device->partition_name) {
        ret = edgehog_nvs_set_str(edgehog_device->partition_name, "part_number", (char *) part_num);
    } else {
        ret = ESP_OK;
    }

end:
    free(previous_value);
    return ret;
}

void edgehog_device_destroy(edgehog_device_handle_t edgehog_device)
{
    if (edgehog_device) {
        astarte_device_destroy(edgehog_device->astarte_device);
        edgehog_battery_status_delete_list(&edgehog_device->battery_list);
        edgehog_geolocation_delete_list(&edgehog_device->geolocation_list);
        edgehog_telemetry_destroy(edgehog_device->edgehog_telemetry);
    }

    free(edgehog_device);
}

esp_err_t edgehog_device_nvs_open(
    edgehog_device_handle_t edgehog_device, char *namespace, nvs_handle_t *out_handle)
{
    esp_err_t ret = nvs_open_from_partition(
        edgehog_device->partition_name, namespace, NVS_READWRITE, out_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to open %s", edgehog_device->partition_name);
        return ret;
    }

    return ESP_OK;
}

nvs_iterator_t edgehog_device_nvs_entry_find(
    edgehog_device_handle_t edgehog_device, const char *namespace, nvs_type_t type)
{
    return nvs_entry_find(edgehog_device->partition_name, namespace, type);
}

telemetry_periodic edgehog_device_get_telemetry_periodic(telemetry_type_t type)
{
    switch (type) {
        case EDGEHOG_TELEMETRY_HW_INFO:
            return publish_device_hardware_info;
        case EDGEHOG_TELEMETRY_WIFI_SCAN:
            return scan_wifi_ap;
        case EDGEHOG_TELEMETRY_SYSTEM_STATUS:
            return publish_system_status;
        case EDGEHOG_TELEMETRY_STORAGE_USAGE:
            return edgehog_storage_usage_publish;
        case EDGEHOG_TELEMETRY_BATTERY_STATUS:
            return edgehog_battery_status_publish;
        case EDGEHOG_TELEMETRY_GEOLOCATION_INFO:
            return edgehog_geolocation_publish;
        default:
            return NULL;
    }
}

telemetry_type_t edgehog_device_get_telemetry_type(const char *interface_name)
{
    if (strcmp(interface_name, hardware_info_interface.name) == 0) {
        return EDGEHOG_TELEMETRY_HW_INFO;
    } else if (strcmp(interface_name, wifi_scan_result_interface.name) == 0) {
        return EDGEHOG_TELEMETRY_WIFI_SCAN;
    } else if (strcmp(interface_name, system_status_status_interface.name) == 0) {
        return EDGEHOG_TELEMETRY_SYSTEM_STATUS;
    } else if (strcmp(interface_name, storage_usage_interface.name) == 0) {
        return EDGEHOG_TELEMETRY_STORAGE_USAGE;
    } else if (strcmp(interface_name, battery_status_interface.name) == 0) {
        return EDGEHOG_TELEMETRY_BATTERY_STATUS;
    } else if (strcmp(interface_name, geolocation_interface.name) == 0) {
        return EDGEHOG_TELEMETRY_GEOLOCATION_INFO;
    } else {
        return EDGEHOG_TELEMETRY_INVALID;
    }
}

static inline bool compare_mac_address(const uint8_t a[], const uint8_t b[])
{
    return memcmp(a, b, 6) == 0;
}

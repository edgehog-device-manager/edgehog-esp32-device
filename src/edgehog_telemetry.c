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

#include "edgehog_telemetry.h"
#include "astarte_bson.h"
#include "astarte_list.h"
#include "edgehog_device_private.h"
#include <astarte_bson_types.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <string.h>

#define NVS_KEY_PREFIX "eht"
// nvs_key_prefix + 1 (p or e) + 1 (1-9 telemetry_type) increment this if telemetry_type is
// greater than 9
#define NVS_KEY_SIZE 6
#define NVS_KEY_PERIOD(telemetry_type)                                                             \
    char period_key[NVS_KEY_SIZE];                                                                 \
    snprintf(period_key, NVS_KEY_SIZE, "%sp%hhd", NVS_KEY_PREFIX, (int8_t) telemetry_type)

#define NVS_KEY_ENABLE(telemetry_type)                                                             \
    char enable_key[NVS_KEY_SIZE];                                                                 \
    snprintf(enable_key, NVS_KEY_SIZE, "%se%hhd", NVS_KEY_PREFIX, (int8_t) telemetry_type)

#define GET_TELEMETRY_TYPE(enable_key)                                                             \
    telemetry_type_t telemetry_type                                                                \
        = (telemetry_type_t) strtol(&enable_entry_info.key[4], NULL, 10);

#define TELEMETRY_NAMESPACE "ehgd_tlm"

#define TELEMETRY_UPDATE_DEFAULT 0
#define TELEMETRY_UPDATE_DISABLED -1
#define TELEMETRY_UPDATE_ENABLED 1

static const char *TAG = "EDGEHOG_TELEMETRY";

const astarte_interface_t telemetry_config_interface
    = { .name = "io.edgehog.devicemanager.config.Telemetry",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_SERVER,
          .type = TYPE_PROPERTIES };

typedef struct astarte_list_head_t timer_list_head_t;

struct timer_ptr_entry_t
{
    timer_list_head_t head;
    edgehog_device_handle_t edgehog_device;
    telemetry_type_t telemetry_type;
    TimerHandle_t timer_handle;
};

struct edgehog_telemetry_data
{
    SemaphoreHandle_t load_tl_mutex;
    edgehog_device_telemetry_config_t *telemetry_config;
    timer_list_head_t timer_list;
    size_t telemetry_config_len;
};

static edgehog_err_t save_telemetry_to_nvs(edgehog_device_handle_t edgehog_device,
    telemetry_type_t telemetry_type, int64_t period_seconds);
static edgehog_err_t telemetry_schedule(edgehog_device_handle_t edgehog_device,
    telemetry_type_t telemetry_type, int64_t period_seconds);
static bool telemetry_type_is_present_in_config(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type);
static int64_t get_telemetry_period_from_config(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type);
static void load_telemetry_from_config(edgehog_device_handle_t edgehog_device);
static void load_telemetry_from_nvs(edgehog_device_handle_t edgehog_device);
static int64_t get_telemetry_period_from_nvs(
    edgehog_device_handle_t edgehog_device, telemetry_type_t telemetry_type);
static struct timer_ptr_entry_t *get_timer_entry(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type);

edgehog_telemetry_t *edgehog_telemetry_new(
    edgehog_device_telemetry_config_t *telemetry_config, size_t telemetry_config_len)
{
    edgehog_telemetry_t *edgehog_telemetry = calloc(1, sizeof(edgehog_telemetry_t));

    if (!edgehog_telemetry) {
        ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
        return NULL;
    }

    edgehog_telemetry->load_tl_mutex = xSemaphoreCreateMutex();
    if (!edgehog_telemetry->load_tl_mutex) {
        ESP_LOGE(TAG, "Cannot create load_tl_mutex");
        goto error;
    }

    edgehog_telemetry->telemetry_config_len = telemetry_config_len;

    if (edgehog_telemetry->telemetry_config_len > 0) {
        edgehog_telemetry->telemetry_config = calloc(
            edgehog_telemetry->telemetry_config_len, sizeof(edgehog_device_telemetry_config_t));
        if (!edgehog_telemetry->telemetry_config) {
            ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
            goto error;
        }
        memcpy(edgehog_telemetry->telemetry_config, telemetry_config,
            telemetry_config_len * sizeof(edgehog_device_telemetry_config_t));
    }

    astarte_list_init(&edgehog_telemetry->timer_list);

    return edgehog_telemetry;

error:
    free(edgehog_telemetry->telemetry_config);
    if (edgehog_telemetry->load_tl_mutex) {
        vSemaphoreDelete(edgehog_telemetry->load_tl_mutex);
    }
    free(edgehog_telemetry);
    return NULL;
}

edgehog_err_t edgehog_telemetry_start(
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry)
{
    if (!edgehog_telemetry) {
        ESP_LOGE(TAG, "Unable to start telemetry, reference is null");
        return EDGEHOG_ERR;
    }

    if (xSemaphoreTake(edgehog_telemetry->load_tl_mutex, (TickType_t) 10) == pdFALSE) {
        ESP_LOGE(TAG, "Unable to start telemetry on device that is being initialized");
        return EDGEHOG_ERR_DEVICE_NOT_READY;
    }

    load_telemetry_from_nvs(edgehog_device);
    load_telemetry_from_config(edgehog_device);

    xSemaphoreGive(edgehog_telemetry->load_tl_mutex);
    return EDGEHOG_OK;
}

static void timer_callback(TimerHandle_t timer_handle)
{
    struct timer_ptr_entry_t *timer_entry
        = (struct timer_ptr_entry_t *) pvTimerGetTimerID(timer_handle);
    telemetry_periodic telemetry_periodic_fn
        = edgehog_device_get_telemetry_periodic(timer_entry->telemetry_type);
    telemetry_periodic_fn(timer_entry->edgehog_device);
}

static edgehog_err_t telemetry_schedule(
    edgehog_device_handle_t edgehog_device, telemetry_type_t telemetry_type, int64_t period_seconds)
{
    if (telemetry_type <= EDGEHOG_TELEMETRY_INVALID) {
        ESP_LOGE(TAG, "Telemetry type invalid %d", telemetry_type);
        goto error;
    }

    NVS_KEY_ENABLE(telemetry_type);
    struct timer_ptr_entry_t *timer_entry
        = get_timer_entry(edgehog_device->edgehog_telemetry, telemetry_type);

    if (!timer_entry) {
        if (period_seconds <= 0) {
            ESP_LOGW(TAG, "timer %d disabled", telemetry_type);
            return EDGEHOG_OK;
        }

        timer_entry = calloc(1, sizeof(struct timer_ptr_entry_t));
        if (!timer_entry) {
            ESP_LOGE(TAG, "Unable to create timer entry %s", enable_key);
            goto error;
        }

        timer_entry->edgehog_device = edgehog_device;
        timer_entry->telemetry_type = telemetry_type;

        timer_entry->timer_handle = xTimerCreate(NULL, pdMS_TO_TICKS(period_seconds * 1000), pdTRUE,
            (void *) timer_entry, timer_callback);
        if (!timer_entry->timer_handle) {
            ESP_LOGE(TAG, "Unable to create timer %s", enable_key);
            free(timer_entry);
            goto error;
        }

        if (xTimerStart(timer_entry->timer_handle, 0) != pdPASS) {
            ESP_LOGW(TAG, "The timer %s could not be set into the Active state", enable_key);
            xTimerDelete(timer_entry->timer_handle, 0);
            free(timer_entry);
            save_telemetry_to_nvs(edgehog_device, telemetry_type, TELEMETRY_UPDATE_DISABLED);
            goto error;
        }

        astarte_list_append(&edgehog_device->edgehog_telemetry->timer_list, &timer_entry->head);
    } else if (period_seconds > 0) {
        xTimerChangePeriod(timer_entry->timer_handle, pdMS_TO_TICKS(period_seconds * 1000), 0);
    } else {
        xTimerDelete(timer_entry->timer_handle, 0);
        astarte_list_remove(&timer_entry->head);
        ESP_LOGI(TAG, "Telemetry type %d removed", telemetry_type);
    }

    save_telemetry_to_nvs(edgehog_device, telemetry_type, period_seconds);
    return EDGEHOG_OK;

error:
    ESP_LOGE(TAG, "Unable to schedule new telemetry");
    return EDGEHOG_ERR;
}

edgehog_err_t edgehog_telemetry_config_event(astarte_device_data_event_t *event_request,
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry)
{
    if (!event_request->path) {
        ESP_LOGW(TAG, "Unable to handle telemetry config request path empty");
        return EDGEHOG_ERR;
    }

    char *save_ptr;
    strtok_r((char *) event_request->path, "/", &save_ptr); // skip /request
    const char *interface_name = strtok_r(NULL, "/", &save_ptr);
    const char *endpoint = strtok_r(NULL, "/", &save_ptr);

    if (!interface_name || !endpoint) {
        ESP_LOGE(TAG, "Unable to handle config telemetry update, parameter empty");
        return EDGEHOG_ERR;
    }

    telemetry_type_t telemetry_type = edgehog_device_get_telemetry_type(interface_name);
    if (telemetry_type == EDGEHOG_TELEMETRY_INVALID) {
        ESP_LOGE(TAG, "Unable to handle config telemetry update, telemetry type %s not supported",
            interface_name);
        return EDGEHOG_ERR;
    }

    if (xSemaphoreTake(edgehog_telemetry->load_tl_mutex, (TickType_t) 10) == pdFALSE) {
        ESP_LOGE(
            TAG, "Trying to handle config telemetry event on device that is being initialized");
        return EDGEHOG_ERR_DEVICE_NOT_READY;
    }

    int64_t period_seconds = get_telemetry_period_from_nvs(edgehog_device, telemetry_type);
    if (strcmp(endpoint, "enable") == 0) {
        bool enable;
        if (event_request->bson_value && event_request->bson_value_type == BSON_TYPE_BOOLEAN) {
            enable = astarte_bson_value_to_int8(event_request->bson_value);
        } else {
            enable = telemetry_type_is_present_in_config(edgehog_telemetry, telemetry_type);
        }

        if (!enable) {
            period_seconds = TELEMETRY_UPDATE_DISABLED;
        }
    } else if (strcmp(endpoint, "periodSeconds") == 0) {
        if (event_request->bson_value && event_request->bson_value_type >= BSON_TYPE_INT32) {
            if (event_request->bson_value_type == BSON_TYPE_INT32) {
                period_seconds = astarte_bson_value_to_int32(event_request->bson_value);
            } else {
                period_seconds = astarte_bson_value_to_int64(event_request->bson_value);
            }
        } else {
            period_seconds = get_telemetry_period_from_config(edgehog_telemetry, telemetry_type);
        }
    }

    telemetry_schedule(edgehog_device, telemetry_type, period_seconds);
    xSemaphoreGive(edgehog_telemetry->load_tl_mutex);

    return EDGEHOG_OK;
}

void edgehog_telemetry_destroy(edgehog_telemetry_t *edgehog_telemetry)
{
    if (edgehog_telemetry) {

        timer_list_head_t *item, *tmp;
        MUTABLE_LIST_FOR_EACH(item, tmp, &edgehog_telemetry->timer_list)
        {
            struct timer_ptr_entry_t *entry = GET_LIST_ENTRY(item, struct timer_ptr_entry_t, head);
            if (xTimerIsTimerActive(entry->timer_handle)) {
                xTimerDelete(entry->timer_handle, 0);
            }
            free(entry);
        }

        free(edgehog_telemetry->telemetry_config);
        vSemaphoreDelete(edgehog_telemetry->load_tl_mutex);
        free(edgehog_telemetry);
    }
}

static edgehog_err_t save_telemetry_to_nvs(
    edgehog_device_handle_t edgehog_device, telemetry_type_t telemetry_type, int64_t period_seconds)
{
    if (telemetry_type <= EDGEHOG_TELEMETRY_INVALID) {
        ESP_LOGE(TAG, "Unable to save telemetry tl_updates invalid type");
        return EDGEHOG_ERR;
    }

    nvs_handle_t nvs_handle;
    esp_err_t nvs_opn_result
        = edgehog_device_nvs_open(edgehog_device, TELEMETRY_NAMESPACE, &nvs_handle);
    edgehog_err_t edgehog_result;
    if (nvs_opn_result != ESP_OK && nvs_opn_result != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Unable to open NVS to save new telemetry update");
        edgehog_result = EDGEHOG_ERR;
    } else {
        NVS_KEY_ENABLE(telemetry_type);
        if (period_seconds > 0) {
            nvs_set_i8(nvs_handle, enable_key, TELEMETRY_UPDATE_ENABLED);
            NVS_KEY_PERIOD(telemetry_type);
            nvs_set_i64(nvs_handle, period_key, period_seconds);
        } else if (period_seconds < 0) {
            nvs_set_i8(nvs_handle, enable_key, TELEMETRY_UPDATE_DISABLED);
        } else {
            nvs_set_i8(nvs_handle, enable_key, TELEMETRY_UPDATE_DEFAULT);
        }
        edgehog_result = EDGEHOG_OK;
    }

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return edgehog_result;
}

static bool telemetry_type_is_present_in_config(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type)
{
    for (int i = 0; i < edgehog_telemetry->telemetry_config_len; i++) {
        if (telemetry_type == edgehog_telemetry->telemetry_config[i].type) {
            return true;
        }
    }
    return false;
}

static int64_t get_telemetry_period_from_config(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type)
{
    for (int i = 0; i < edgehog_telemetry->telemetry_config_len; i++) {
        if (telemetry_type == edgehog_telemetry->telemetry_config[i].type) {
            return edgehog_telemetry->telemetry_config[i].period_seconds;
        }
    }
    return -1;
}

static int64_t get_telemetry_period_from_nvs(
    edgehog_device_handle_t edgehog_device, telemetry_type_t telemetry_type)
{
    nvs_handle_t nvs_handle;
    esp_err_t result = edgehog_device_nvs_open(edgehog_device, TELEMETRY_NAMESPACE, &nvs_handle);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Unable to open nvs for loading telemetry error %s", esp_err_to_name(result));
        return TELEMETRY_UPDATE_DISABLED;
    }

    int64_t period_seconds = TELEMETRY_UPDATE_DEFAULT;
    NVS_KEY_PERIOD(telemetry_type);
    nvs_get_i64(nvs_handle, period_key, &period_seconds);
    nvs_close(nvs_handle);
    return period_seconds;
}

static void load_telemetry_from_config(edgehog_device_handle_t edgehog_device)
{
    edgehog_telemetry_t *edgehog_telemetry = edgehog_device->edgehog_telemetry;
    for (int i = 0; i < edgehog_telemetry->telemetry_config_len; i++) {
        edgehog_device_telemetry_config_t telemetry_config = edgehog_telemetry->telemetry_config[i];
        struct timer_ptr_entry_t *timer_entry
            = get_timer_entry(edgehog_device->edgehog_telemetry, telemetry_config.type);
        if (!timer_entry) {
            telemetry_schedule(
                edgehog_device, telemetry_config.type, telemetry_config.period_seconds);
        }
    }
}

static void load_telemetry_from_nvs(edgehog_device_handle_t edgehog_device)
{
    nvs_handle_t nvs_handle;
    esp_err_t result = edgehog_device_nvs_open(edgehog_device, TELEMETRY_NAMESPACE, &nvs_handle);

    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Unable to open nvs for loading telemetry error %s", esp_err_to_name(result));
        return;
    }

    for (nvs_iterator_t it
         = edgehog_device_nvs_entry_find(edgehog_device, TELEMETRY_NAMESPACE, NVS_TYPE_I8);
         it; it = nvs_entry_next(it)) {
        nvs_entry_info_t enable_entry_info;
        nvs_entry_info(it, &enable_entry_info);

        if (strncmp(enable_entry_info.key, NVS_KEY_PREFIX, strlen(NVS_KEY_PREFIX)) != 0) {
            continue;
        }

        GET_TELEMETRY_TYPE(enable_entry_info.key);
        NVS_KEY_PERIOD(telemetry_type);
        int8_t enable = TELEMETRY_UPDATE_DEFAULT;
        nvs_get_i8(nvs_handle, enable_entry_info.key, &enable);

        if (enable == TELEMETRY_UPDATE_DEFAULT) {
            continue;
        }

        int64_t period_seconds = TELEMETRY_UPDATE_DEFAULT;
        nvs_get_i64(nvs_handle, period_key, &period_seconds);

        if (enable == TELEMETRY_UPDATE_DISABLED) {
            period_seconds = TELEMETRY_UPDATE_DISABLED;
        } else {
            ESP_LOGI(TAG, "Load telemetry config type %d %d %lld from nvs", telemetry_type, enable,
                period_seconds);
        }

        telemetry_schedule(edgehog_device, telemetry_type, period_seconds);
    }

    nvs_close(nvs_handle);
}

static struct timer_ptr_entry_t *get_timer_entry(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type)
{
    timer_list_head_t *item;
    NVS_KEY_ENABLE(telemetry_type);
    LIST_FOR_EACH(item, &edgehog_telemetry->timer_list)
    {
        struct timer_ptr_entry_t *entry = GET_LIST_ENTRY(item, struct timer_ptr_entry_t, head);
        TimerHandle_t timer_handle = entry->timer_handle;
        if (timer_handle && telemetry_type == entry->telemetry_type) {
            return entry;
        }
    }
    return NULL;
}

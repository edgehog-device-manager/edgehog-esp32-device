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

#include "edgehog_telemetry.h"
#include "astarte_bson.h"
#include "edgehog_device_private.h"
#include <astarte_bson_types.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>

#define TASK_NAME_PREFIX "eht"
// task_name_prefix + 1 (p or e) + 1 (1-9 telemetry_type) increment this if telemetry_type is
// greater than 9
#define TASK_NAME_SIZE 6
#define NVS_KEY_PERIOD(telemetry_type)                                                             \
    char period_key[TASK_NAME_SIZE];                                                               \
    snprintf(period_key, TASK_NAME_SIZE, "%sp%hhd", TASK_NAME_PREFIX, (int8_t) telemetry_type)

#define NVS_KEY_ENABLE(telemetry_type)                                                             \
    char enable_key[TASK_NAME_SIZE];                                                               \
    snprintf(enable_key, TASK_NAME_SIZE, "%se%hhd", TASK_NAME_PREFIX, (int8_t) telemetry_type)

#define GET_TELEMETRY_INDEX(telemetry_type) ((int) telemetry_type - 1)
#define GET_TELEMETRY_TYPE(telemetry_index) ((telemetry_type_t)(telemetry_index + 1))

#define TELEMETRY_NAMESPACE "ehgd_tlm"

// The telemetry update array length, increment this when add item to telemetry_type_t
#define TELEMETRY_TASK_PARAM_LEN 3
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

struct telemetry_task_param
{
    edgehog_device_handle_t edgehog_device;
    telemetry_type_t type;
    int64_t period_seconds;
    TaskHandle_t x_handle;
};

struct edgehog_telemetry_data
{
    SemaphoreHandle_t load_tl_mutex;
    struct telemetry_task_param tl_params[TELEMETRY_TASK_PARAM_LEN];
    edgehog_device_telemetry_config_t *telemetry_config;
    size_t telemetry_config_len;
};

static void telemetry_loop(void *task_param);
static struct telemetry_task_param *get_telemetry_task_param(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type);
static edgehog_err_t save_telemetry_task_param_to_nvs(
    edgehog_device_handle_t edgehog_device, struct telemetry_task_param *telemetry_task_param);
static edgehog_err_t telemetry_schedule(
    edgehog_device_handle_t edgehog_device, struct telemetry_task_param *telemetry_task_param);
static bool telemetry_type_is_present_in_config(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type);
static int64_t get_telemetry_period_from_config(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type);
static void load_telemetry_task_params_from_config(
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry);
static void load_telemetry_task_params_from_nvs(
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry);
static bool is_task_created(xTaskHandle handle);
static void telemetry_update_init(
    struct telemetry_task_param *telemetry_task_param, telemetry_type_t telemetry_type);
static int64_t get_telemetry_period_from_nvs(
    edgehog_device_handle_t edgehog_device, telemetry_type_t telemetry_type);

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

    for (int i = 0; i < TELEMETRY_TASK_PARAM_LEN; i++) {
        telemetry_update_init(
            &edgehog_telemetry->tl_params[i], (telemetry_type_t) GET_TELEMETRY_TYPE(i));
    }

    if (telemetry_config_len > 0) {
        edgehog_telemetry->telemetry_config_len = telemetry_config_len;

        edgehog_telemetry->telemetry_config = calloc(
            edgehog_telemetry->telemetry_config_len, sizeof(edgehog_device_telemetry_config_t));
        if (!edgehog_telemetry->telemetry_config) {
            ESP_LOGE(TAG, "Out of memory %s: %d", __FILE__, __LINE__);
            goto error;
        }
        memcpy(edgehog_telemetry->telemetry_config, telemetry_config,
            telemetry_config_len * sizeof(edgehog_device_telemetry_config_t));
    } else {
        edgehog_telemetry->telemetry_config_len = 0;
    }
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

    load_telemetry_task_params_from_config(edgehog_device, edgehog_telemetry);
    load_telemetry_task_params_from_nvs(edgehog_device, edgehog_telemetry);

    for (int i = 0; i < TELEMETRY_TASK_PARAM_LEN; i++) {
        struct telemetry_task_param *telemetry_task_param = &edgehog_telemetry->tl_params[i];
        save_telemetry_task_param_to_nvs(edgehog_device, telemetry_task_param);
        telemetry_schedule(edgehog_device, telemetry_task_param);
    }

    xSemaphoreGive(edgehog_telemetry->load_tl_mutex);
    return EDGEHOG_OK;
}

static edgehog_err_t telemetry_schedule(
    edgehog_device_handle_t edgehog_device, struct telemetry_task_param *telemetry_task_param)
{
    bool is_created = is_task_created(telemetry_task_param->x_handle);
    if (is_created) {
        vTaskDelete(telemetry_task_param->x_handle);
        ESP_LOGI(TAG, "Telemetry tl_updates type %d removed", telemetry_task_param->type);
    }

    telemetry_task_param->x_handle = NULL;

    if (telemetry_task_param->period_seconds <= 0) {
        save_telemetry_task_param_to_nvs(edgehog_device, telemetry_task_param);
        return EDGEHOG_OK;
    }

    if (telemetry_task_param->type <= EDGEHOG_TELEMETRY_INVALID) {
        ESP_LOGE(TAG, "Telemetry tl_updates invalid %d", telemetry_task_param->type);
        goto error;
    }

    NVS_KEY_ENABLE(telemetry_task_param->type);
    if (xTaskCreate(telemetry_loop, enable_key, 4096, telemetry_task_param, tskIDLE_PRIORITY,
            &telemetry_task_param->x_handle)
        != pdPASS) {
        telemetry_task_param->period_seconds = TELEMETRY_UPDATE_DISABLED;
        save_telemetry_task_param_to_nvs(edgehog_device, telemetry_task_param);
        goto error;
    }

    return EDGEHOG_OK;

error:
    ESP_LOGE(TAG, "Unable to schedule new telemetry");
    return EDGEHOG_ERR;
}

static void telemetry_loop(void *task_param)
{
    struct telemetry_task_param *param = (struct telemetry_task_param *) task_param;
    ESP_LOGI(
        TAG, "Telemetry active on type %d every %lld seconds", param->type, param->period_seconds);
    telemetry_periodic periodic_fn = edgehog_device_get_telemetry_periodic(param->type);
    for (;;) {
        periodic_fn(param->edgehog_device);
        vTaskDelay(pdMS_TO_TICKS(param->period_seconds * 1000));
    }
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

    struct telemetry_task_param *telemetry_task_param
        = get_telemetry_task_param(edgehog_telemetry, telemetry_type);
    if (!telemetry_task_param) {
        ESP_LOGE(TAG, "Unable to update telemetry for type %d", telemetry_type);
        return EDGEHOG_ERR;
    }

    if (xSemaphoreTake(edgehog_telemetry->load_tl_mutex, (TickType_t) 10) == pdFALSE) {
        ESP_LOGE(
            TAG, "Trying to handle config telemetry event on device that is being initialized");
        return EDGEHOG_ERR_DEVICE_NOT_READY;
    }

    if (strcmp(endpoint, "enable") == 0) {
        bool enable;
        if (event_request->bson_value && event_request->bson_value_type == BSON_TYPE_BOOLEAN) {
            enable = astarte_bson_value_to_int8(event_request->bson_value);
        } else {
            enable = telemetry_type_is_present_in_config(edgehog_telemetry, telemetry_type);
        }

        if (enable) {
            telemetry_task_param->period_seconds
                = get_telemetry_period_from_nvs(edgehog_device, telemetry_type);
        } else {
            telemetry_task_param->period_seconds = TELEMETRY_UPDATE_DISABLED;
        }

        save_telemetry_task_param_to_nvs(edgehog_device, telemetry_task_param);
    } else if (strcmp(endpoint, "periodSeconds") == 0) {
        int64_t period_seconds;
        if (event_request->bson_value && event_request->bson_value_type >= BSON_TYPE_INT32) {
            if (event_request->bson_value_type == BSON_TYPE_INT32) {
                period_seconds = astarte_bson_value_to_int32(event_request->bson_value);
            } else {
                period_seconds = astarte_bson_value_to_int64(event_request->bson_value);
            }
        } else {
            period_seconds = get_telemetry_period_from_config(edgehog_telemetry, telemetry_type);
        }

        telemetry_task_param->period_seconds = period_seconds;
        save_telemetry_task_param_to_nvs(edgehog_device, telemetry_task_param);
    }

    telemetry_schedule(edgehog_device, telemetry_task_param);
    xSemaphoreGive(edgehog_telemetry->load_tl_mutex);

    return EDGEHOG_OK;
}

void edgehog_telemetry_destroy(edgehog_telemetry_t *edgehog_telemetry)
{
    if (edgehog_telemetry) {
        for (int i = 0; i < TELEMETRY_TASK_PARAM_LEN; i++) {
            if (is_task_created(edgehog_telemetry->tl_params[i].x_handle)) {
                vTaskDelete(edgehog_telemetry->tl_params[i].x_handle);
            }
        }
        free(edgehog_telemetry->tl_params);
        free(edgehog_telemetry->telemetry_config);
        vSemaphoreDelete(edgehog_telemetry->load_tl_mutex);
        free(edgehog_telemetry);
    }
}

static bool is_task_created(xTaskHandle handle)
{
    if (!handle) {
        return false;
    }

    const char *task_get_name = pcTaskGetTaskName(handle);
    return task_get_name && strlen(task_get_name) > 0;
}

static struct telemetry_task_param *get_telemetry_task_param(
    edgehog_telemetry_t *edgehog_telemetry, telemetry_type_t telemetry_type)
{
    int task_handle_index = GET_TELEMETRY_INDEX(telemetry_type);
    if (task_handle_index == -1) {
        ESP_LOGE(TAG, "Unable to get telemetry, type unsupported");
        return NULL;
    }
    return &edgehog_telemetry->tl_params[task_handle_index];
}

static void telemetry_update_init(
    struct telemetry_task_param *telemetry_task_param, telemetry_type_t telemetry_type)
{
    telemetry_task_param->type = telemetry_type;
    telemetry_task_param->x_handle = NULL;
    telemetry_task_param->period_seconds = TELEMETRY_UPDATE_DEFAULT;
}

static edgehog_err_t save_telemetry_task_param_to_nvs(
    edgehog_device_handle_t edgehog_device, struct telemetry_task_param *telemetry_task_param)
{
    if (telemetry_task_param->type <= EDGEHOG_TELEMETRY_INVALID) {
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

        NVS_KEY_ENABLE(telemetry_task_param->type);
        if (telemetry_task_param->period_seconds > 0) {
            nvs_set_i8(nvs_handle, enable_key, TELEMETRY_UPDATE_ENABLED);
            NVS_KEY_PERIOD(telemetry_task_param->type);
            nvs_set_i64(nvs_handle, period_key, telemetry_task_param->period_seconds);
        } else if (telemetry_task_param->period_seconds < 0) {
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

static void load_telemetry_task_params_from_config(
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry)
{
    for (int i = 0; i < edgehog_telemetry->telemetry_config_len; i++) {
        struct telemetry_task_param *telemetry_task_param = get_telemetry_task_param(
            edgehog_telemetry, edgehog_telemetry->telemetry_config[i].type);

        if (!telemetry_task_param) {
            continue;
        }

        telemetry_task_param->period_seconds
            = edgehog_telemetry->telemetry_config[i].period_seconds;
        telemetry_task_param->edgehog_device = edgehog_device;
    }
}

static void load_telemetry_task_params_from_nvs(
    edgehog_device_handle_t edgehog_device, edgehog_telemetry_t *edgehog_telemetry)
{
    nvs_handle_t nvs_handle;
    esp_err_t result = edgehog_device_nvs_open(edgehog_device, TELEMETRY_NAMESPACE, &nvs_handle);

    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Unable to open nvs for loading telemetry error %s", esp_err_to_name(result));
        return;
    }

    for (int i = 0; i < TELEMETRY_TASK_PARAM_LEN; i++) {
        telemetry_type_t telemetry_type = (telemetry_type_t) GET_TELEMETRY_TYPE(i);
        struct telemetry_task_param *telemetry_task_param
            = get_telemetry_task_param(edgehog_telemetry, telemetry_type);
        if (!telemetry_task_param) {
            ESP_LOGE(TAG, "Unable to update telemetry for type %d", telemetry_type);
            continue;
        }

        NVS_KEY_ENABLE(telemetry_type);
        NVS_KEY_PERIOD(telemetry_type);
        int8_t enable = TELEMETRY_UPDATE_DEFAULT;
        nvs_get_i8(nvs_handle, enable_key, &enable);
        int64_t period_seconds = TELEMETRY_UPDATE_DEFAULT;
        nvs_get_i64(nvs_handle, period_key, &period_seconds);
        if (enable == TELEMETRY_UPDATE_ENABLED) {
            telemetry_task_param->period_seconds = period_seconds;
            telemetry_task_param->edgehog_device = edgehog_device;
            ESP_LOGI(TAG, "Load telemetry config type %d %d %lld from nvs", telemetry_type, enable,
                period_seconds);
        } else if (enable == TELEMETRY_UPDATE_DISABLED) {
            telemetry_task_param->period_seconds = TELEMETRY_UPDATE_DISABLED;
        }
    }

    nvs_close(nvs_handle);
}

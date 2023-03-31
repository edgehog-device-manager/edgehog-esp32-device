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

#include "edgehog_ota.h"
#include "edgehog_device_private.h"
#include "edgehog_event.h"
#include <astarte_bson.h>
#include <astarte_bson_serializer.h>
#include <astarte_bson_types.h>
#include <esp_err.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <freertos/task.h>
#include <nvs.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <spi_flash_mmap.h>
#endif

#define OTA_REQ_TIMEOUT_MS (60 * 1000)
#define MAX_OTA_RETRY 5
#define OTA_NAMESPACE "edgehog_ota"
#define OTA_STATE_KEY "state"
#define OTA_PARTITION_ADDR_KEY "part_id"
#define OTA_REQUEST_ID_KEY "req_id"

static const char *TAG = "EDGEHOG_OTA";

typedef enum
{
    OTA_IDLE,
    OTA_DOWNLOAD_DEPLOY,
    OTA_REBOOT,
    OTA_FAILED,
    OTA_SUCCESS
} edgehog_ota_state;

const astarte_interface_t ota_request_interface = { .name = "io.edgehog.devicemanager.OTARequest",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_SERVER,
    .type = TYPE_DATASTREAM };

const astarte_interface_t ota_response_interface = { .name = "io.edgehog.devicemanager.OTAResponse",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_DATASTREAM };

static bool is_partition_changed(nvs_handle handle);
static void publish_ota_data(astarte_device_handle_t astarte_device, const char *request_uuid,
    edgehog_ota_state state, edgehog_err_t error);
/**
 * This function is blocking, the calling task will be blocked until the OTA procedure completes.
 */
static edgehog_err_t do_ota(
    edgehog_device_handle_t edgehog_device, const char *request_uuid, const char *ota_url);
static void ota_state_reboot(nvs_handle handle);
static void ota_state_failed(astarte_device_handle_t astarte_device, nvs_handle handle,
    const char *request_uuid, edgehog_err_t ota_result);
static const char *edgehog_ota_state_to_string(edgehog_ota_state state);
static const char *edgehog_err_to_code(edgehog_err_t error);

static esp_err_t http_ota_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(
                TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "DOWNLOAD FINISHED");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void edgehog_ota_init(edgehog_device_handle_t edgehog_device)
{
    nvs_handle_t handle;
    esp_err_t result = edgehog_device_nvs_open(edgehog_device, OTA_NAMESPACE, &handle);

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Missing OTA namespace in NVS, if there is no pending OTA ignore it");
        return;
    }

    char req_uuid[ASTARTE_UUID_LEN];
    size_t req_uuid_size = ASTARTE_UUID_LEN;
    result = nvs_get_str(handle, OTA_REQUEST_ID_KEY, req_uuid, &req_uuid_size);

    if (result != ESP_OK) {
        goto end;
    }

    uint8_t ota_state = OTA_IDLE;
    result = nvs_get_u8(handle, OTA_STATE_KEY, &ota_state);

    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;
    if (result != ESP_OK || ota_state != OTA_REBOOT) {
        nvs_set_u8(handle, OTA_STATE_KEY, OTA_FAILED);
        nvs_commit(handle);
        publish_ota_data(astarte_device, req_uuid, OTA_FAILED, EDGEHOG_ERR);
        goto end;
    }

    if (is_partition_changed(handle)) {
        publish_ota_data(astarte_device, req_uuid, OTA_SUCCESS, EDGEHOG_OK);
    } else {
        ESP_LOGE(TAG, "Unable to switch into updated partition");
        nvs_set_u8(handle, OTA_STATE_KEY, OTA_FAILED);
        nvs_commit(handle);
        publish_ota_data(astarte_device, req_uuid, OTA_FAILED, EDGEHOG_ERR_OTA_WRONG_PARTITION);
    }

end:
    nvs_erase_key(handle, OTA_REQUEST_ID_KEY);
    nvs_set_u8(handle, OTA_STATE_KEY, OTA_IDLE);
    nvs_commit(handle);
    nvs_close(handle);
}

// Beware this function blocks the caller until OTA is completed.
edgehog_err_t edgehog_ota_event(
    edgehog_device_handle_t edgehog_device, astarte_device_data_event_t *event_request)
{
    EDGEHOG_VALIDATE_INCOMING_DATA(TAG, event_request, "/request", BSON_TYPE_DOCUMENT);

    uint8_t type;
    size_t str_value_len;

    const void *found = astarte_bson_key_lookup("uuid", event_request->bson_value, &type);
    const char *request_uuid = astarte_bson_value_to_string(found, &str_value_len);
    if (!request_uuid || type != BSON_TYPE_STRING) {
        ESP_LOGE(TAG, "Unable to extract requestUUID from bson");
        return EDGEHOG_ERR;
    }

    found = astarte_bson_key_lookup("url", event_request->bson_value, &type);
    const char *ota_url = astarte_bson_value_to_string(found, &str_value_len);
    if (!ota_url || type != BSON_TYPE_STRING) {
        ESP_LOGE(TAG, "Unable to extract URL from bson");
        return EDGEHOG_ERR;
    }

    // Beware this function blocks the caller until OTA is completed.
    return do_ota(edgehog_device, request_uuid, ota_url);
}

// TODO: make this function async using advanced_esp_ota instead of esp_https_ota
static edgehog_err_t do_ota(
    edgehog_device_handle_t edgehog_device, const char *request_uuid, const char *ota_url)
{
    ESP_LOGI(TAG, "INIT");
    esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_INIT_EVENT, NULL, 0, 0);

    nvs_handle_t handle;
    esp_err_t esp_ret = edgehog_device_nvs_open(edgehog_device, OTA_NAMESPACE, &handle);
    astarte_device_handle_t astarte_device = edgehog_device->astarte_device;

    if (esp_ret != ESP_OK && esp_ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Unable to open NVS to save ota state, ota cancelled");
        publish_ota_data(astarte_device, request_uuid, OTA_FAILED, EDGEHOG_ERR_NVS);
        goto error;
    }

    uint8_t ota_state = OTA_IDLE;
    esp_ret = nvs_get_u8(handle, OTA_STATE_KEY, &ota_state);

    if (esp_ret == ESP_OK && ota_state != OTA_IDLE) {
        ESP_LOGE(TAG, "Unable to do OTA Operation, OTA already in progress");
        publish_ota_data(
            astarte_device, request_uuid, OTA_FAILED, EDGEHOG_ERR_OTA_ALREADY_IN_PROGRESS);
        goto error;
    }

    esp_ret = nvs_set_str(handle, OTA_REQUEST_ID_KEY, request_uuid);
    nvs_commit(handle);

    if (esp_ret != ESP_OK && esp_ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Unable to write OTA request_uuid into NVS, ota cancelled");
        publish_ota_data(astarte_device, request_uuid, OTA_FAILED, EDGEHOG_ERR_NVS);
        goto error;
    }

    ota_state = OTA_DOWNLOAD_DEPLOY;
    publish_ota_data(astarte_device, request_uuid, ota_state, EDGEHOG_OK);
    nvs_set_u8(handle, OTA_STATE_KEY, ota_state);
    nvs_commit(handle);

    ESP_LOGI(TAG, "DOWNLOAD_AND_DEPLOY");
    esp_http_client_config_t config = {
        .url = ota_url,
        .event_handler = http_ota_event_handler,
        .timeout_ms = OTA_REQ_TIMEOUT_MS,
    };

    uint8_t attempts = 0;
    // FIXME: this function is blocking
    esp_ret = esp_https_ota(&config);
    while (attempts < MAX_OTA_RETRY && esp_ret != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(attempts * 2000));
        attempts++;
        ESP_LOGW(TAG, "! OTA FAILED, ATTEMPT #%d !", attempts);
        esp_ret = esp_https_ota(&config);
    }

    ESP_LOGI(TAG, "RESULT %s", esp_err_to_name(esp_ret));

    edgehog_err_t ota_result;
    switch (esp_ret) {
        case ESP_OK:
            ota_result = EDGEHOG_OK;
            ota_state_reboot(handle);
            break;
        case ESP_ERR_INVALID_ARG:
            ota_result = EDGEHOG_ERR_NETWORK;
            ota_state_failed(astarte_device, handle, request_uuid, ota_result);
            break;
        case ESP_ERR_OTA_VALIDATE_FAILED:
        case ESP_ERR_INVALID_SIZE:
        case ESP_ERR_NO_MEM:
        case ESP_ERR_FLASH_OP_TIMEOUT:
        case ESP_ERR_FLASH_OP_FAIL:
        case ESP_ERR_FLASH_BASE:
            ota_result = EDGEHOG_ERR_OTA_DEPLOY;
            ota_state_failed(astarte_device, handle, request_uuid, ota_result);
            break;
        default:
            ota_result = EDGEHOG_ERR_OTA_FAILED;
            ota_state_failed(astarte_device, handle, request_uuid, ota_result);
    }

    nvs_close(handle);
    return ota_result;

error:
    nvs_close(handle);
    return EDGEHOG_ERR_OTA_FAILED;
}

static void ota_state_reboot(nvs_handle handle)
{
    nvs_set_u8(handle, OTA_STATE_KEY, OTA_REBOOT);
    nvs_commit(handle);
    const esp_partition_t *partition_info = (esp_partition_t *) esp_ota_get_running_partition();
    nvs_set_u32(handle, OTA_PARTITION_ADDR_KEY, partition_info->address);
    nvs_commit(handle);
}

static void ota_state_failed(astarte_device_handle_t astarte_device, nvs_handle handle,
    const char *request_uuid, edgehog_err_t ota_result)
{
    ESP_LOGW(TAG, "OTA FAILED");
    nvs_set_u8(handle, OTA_STATE_KEY, OTA_FAILED);
    nvs_commit(handle);
    publish_ota_data(astarte_device, request_uuid, OTA_FAILED, ota_result);
    nvs_set_u8(handle, OTA_STATE_KEY, OTA_IDLE);
    nvs_commit(handle);
}

static void publish_ota_data(astarte_device_handle_t astarte_device, const char *request_uuid,
    edgehog_ota_state state, edgehog_err_t error)
{
    const char *str_ota_state = edgehog_ota_state_to_string(state);
    const char *status_code = edgehog_err_to_code(error);

    struct astarte_bson_serializer_t bs;
    astarte_bson_serializer_init(&bs);
    astarte_bson_serializer_append_string(&bs, "uuid", request_uuid);
    astarte_bson_serializer_append_string(&bs, "status", str_ota_state);
    astarte_bson_serializer_append_string(&bs, "statusCode", status_code);
    astarte_bson_serializer_append_end_of_document(&bs);

    int doc_len;
    const void *doc = astarte_bson_serializer_get_document(&bs, &doc_len);
    astarte_device_stream_aggregate(
        astarte_device, ota_response_interface.name, "/response", doc, 0);
    astarte_bson_serializer_destroy(&bs);

    if (state == OTA_SUCCESS) {
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_SUCCESS_EVENT, NULL, 0, 0);
    } else if (state == OTA_FAILED) {
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
    }
}

static bool is_partition_changed(nvs_handle handle)
{
    const esp_partition_t *partition_info = esp_ota_get_running_partition();
    uint32_t prev_partition_addr;

    esp_err_t result = nvs_get_u32(handle, OTA_PARTITION_ADDR_KEY, &prev_partition_addr);
    return result == ESP_OK && prev_partition_addr != partition_info->address;
}

static const char *edgehog_ota_state_to_string(edgehog_ota_state state)
{
    switch (state) {
        case OTA_FAILED:
            return "Error";
        case OTA_SUCCESS:
            return "Done";
        default:
            return "InProgress";
    }
}

static const char *edgehog_err_to_code(edgehog_err_t error)
{
    switch (error) {
        case EDGEHOG_ERR_NETWORK:
            return "OTAErrorNetwork";
        case EDGEHOG_ERR_NVS:
            return "OTAErrorNvs";
        case EDGEHOG_ERR_OTA_ALREADY_IN_PROGRESS:
            return "OTAAlreadyInProgress";
        case EDGEHOG_ERR_OTA_FAILED:
            return "OTAFailed";
        case EDGEHOG_ERR_OTA_DEPLOY:
            return "OTAErrorDeploy";
        case EDGEHOG_ERR_OTA_WRONG_PARTITION:
            return "OTAErrorBootWrongPartition";
        default:
            return "";
    }
}

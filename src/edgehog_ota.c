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
#include <nvs.h>

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#define OTA_REQ_TIMEOUT_MS (60 * 1000)
#define MAX_OTA_RETRY 5
#define OTA_NAMESPACE "edgehog_ota"
#define OTA_STATE_KEY "state"
#define OTA_PARTITION_ADDR_KEY "part_id"
#define OTA_REQUEST_ID_KEY "req_id"
#define OTA_UPDATE_TASK_NAME "OTA UPDATE TASK"
#define OTA_PROGRESS_PERC_ROUNDING_STEP 10

#define TAG "EDGEHOG_OTA"

typedef enum
{
    OTA_STATE_IDLE,
    OTA_STATE_IN_PROGRESS,
    OTA_STATE_REBOOT,
} ota_state_t;

typedef enum
{
    OTA_EVENT_ACKNOWLEDGED,
    OTA_EVENT_DOWNLOADING,
    OTA_EVENT_DEPLOYING,
    OTA_EVENT_DEPLOYED,
    OTA_EVENT_REBOOTING,
    OTA_EVENT_SUCCESS,
    OTA_EVENT_ERROR,
    OTA_EVENT_FAILURE
} ota_event_t;

typedef struct
{
    edgehog_device_handle_t edgehog_dev;
    char *req_uuid;
    char *ota_url;
} ota_task_data_t;

const astarte_interface_t ota_request_interface = { .name = "io.edgehog.devicemanager.OTARequest",
    .major_version = 1,
    .minor_version = 0,
    .ownership = OWNERSHIP_SERVER,
    .type = TYPE_DATASTREAM };

const astarte_interface_t ota_event_interface = { .name = "io.edgehog.devicemanager.OTAEvent",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_DEVICE,
    .type = TYPE_DATASTREAM };

/************************************************
 *               Static variables               *
 ***********************************************/

static ota_task_data_t ota_task_data;

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief OTA update task. Performs the OTA update.
 *
 * @param[in] pvParameters Required information for the OTA update.
 */
static void ota_task_code(void *pvParameters);
/**
 * @brief Perform the OTA update.
 *
 * @param[in] task_data OTA update task data.
 * @param[in] handle_nvs Valid nvs handle.
 *
 * @return EDGEHOG_OK if the update was successful, an edgehog_err_t otherwise.
 */
static edgehog_err_t perform_ota(ota_task_data_t *task_data, nvs_handle_t *handle_nvs);
/**
 * @brief Perform a single attempt to an OTA update.
 *
 * @param[in] task_data OTA update task data.
 *
 * @return EDGEHOG_OK if the update attempt was successful, an edgehog_err_t otherwise.
 */
static edgehog_err_t perform_ota_attempt(ota_task_data_t *task_data);
/**
 * @brief Publish an OTA update event to Astarte.
 *
 * @param[in] edgehog_dev Handle to the edgehog device instance.
 * @param[in] request_uuid Uuid for the OTA request.
 * @param[in] event Event to publish.
 * @param[in] status_progress Percentage of progress for the operation.
 * @param[in] error Possible errors from edgehog generated during the OTA operation.
 * @param[in] message Additional message to append to the OTA update event.
 */
static void pub_ota_event(edgehog_device_handle_t edgehog_dev, const char *request_uuid,
    ota_event_t event, int32_t status_progress, edgehog_err_t error, const char *message);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

void edgehog_ota_init(edgehog_device_handle_t edgehog_dev)
{
    esp_err_t esp_err;
    nvs_handle_t handle_nvs;

    // Step 1 open NVS

    esp_err = edgehog_device_nvs_open(edgehog_dev, OTA_NAMESPACE, &handle_nvs);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening the NVS OTA update namespace.");
        return;
    }

    // Step 2 check if an UUID is present in NVS. If not there is no need to continue as there
    // is no pending OTA update.

    char req_uuid[ASTARTE_UUID_LEN];
    size_t req_uuid_size = ASTARTE_UUID_LEN;
    esp_err = nvs_get_str(handle_nvs, OTA_REQUEST_ID_KEY, req_uuid, &req_uuid_size);
    if (esp_err != ESP_OK) {
        if (esp_err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "Error fetching the OTA update request UUID from NVS.");
        }
        goto end;
    }

    // Step 3 check if the OTA update state is reboot. If not notify astarte of the error.

    uint8_t ota_state;
    esp_err = nvs_get_u8(handle_nvs, OTA_STATE_KEY, &ota_state);
    if (esp_err != ESP_OK || ota_state != OTA_STATE_REBOOT) {
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INTERNAL, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
        goto end;
    }

    // Step 4 check if the previous partition is different from the current one.
    // If not notify astarte of the error.

    uint32_t prev_partition_addr;
    const esp_partition_t *running_partition_info = esp_ota_get_running_partition();
    esp_err = nvs_get_u32(handle_nvs, OTA_PARTITION_ADDR_KEY, &prev_partition_addr);
    if (esp_err == ESP_OK && prev_partition_addr != running_partition_info->address) {
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_SUCCESS, 0, EDGEHOG_OK, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_SUCCESS_EVENT, NULL, 0, 0);
    } else {
        ESP_LOGE(TAG, "Unable to switch into updated partition");
        pub_ota_event(
            edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_SYSTEM_ROLLBACK, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
    }

end:
    nvs_erase_key(handle_nvs, OTA_REQUEST_ID_KEY);
    nvs_set_u8(handle_nvs, OTA_STATE_KEY, OTA_STATE_IDLE);
    nvs_commit(handle_nvs);
    nvs_close(handle_nvs);
}

edgehog_err_t edgehog_ota_event(
    edgehog_device_handle_t edgehog_dev, astarte_device_data_event_t *event_request)
{
    uint8_t type;

    EDGEHOG_VALIDATE_INCOMING_DATA(TAG, event_request, "/request", BSON_TYPE_DOCUMENT);

    // Step 1 get UUID, URL and operation from Astarte request

    const void *found = astarte_bson_key_lookup("uuid", event_request->bson_value, &type);
    uint32_t req_uuid_len;
    const char *req_uuid = astarte_bson_value_to_string(found, &req_uuid_len);
    if (!req_uuid || type != BSON_TYPE_STRING) {
        ESP_LOGE(TAG, "Unable to extract requestUUID from bson");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
        return EDGEHOG_ERR_OTA_INVALID_REQUEST;
    }
    ESP_LOGI(TAG, "OTA UPDATE REQUEST UUID : %s", req_uuid);

    found = astarte_bson_key_lookup("url", event_request->bson_value, &type);
    uint32_t ota_url_len;
    const char *ota_url = astarte_bson_value_to_string(found, &ota_url_len);
    if (!ota_url || type != BSON_TYPE_STRING) {
        ESP_LOGE(TAG, "Unable to extract URL from bson");
        pub_ota_event(
            edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INVALID_REQUEST, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
        return EDGEHOG_ERR_OTA_INVALID_REQUEST;
    }

    found = astarte_bson_key_lookup("operation", event_request->bson_value, &type);
    uint32_t str_value_len;
    const char *ota_operation = astarte_bson_value_to_string(found, &str_value_len);
    if (!ota_operation || type != BSON_TYPE_STRING) {
        ESP_LOGE(TAG, "Unable to extract operation from bson");
        pub_ota_event(
            edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INVALID_REQUEST, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
        return EDGEHOG_ERR_OTA_INVALID_REQUEST;
    }

    // Step 2 Perform the requested Update or Cancel operation.

    if (strcmp("Update", ota_operation) == 0) {
        // Verify that the update is not already in progress
        if (xTaskGetHandle(OTA_UPDATE_TASK_NAME)) {
            pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0,
                EDGEHOG_ERR_OTA_ALREADY_IN_PROGRESS, "");
            return EDGEHOG_ERR_OTA_ALREADY_IN_PROGRESS;
        }
        // Spawn a new task that will perform the update
        ota_task_data.edgehog_dev = edgehog_dev;
        ota_task_data.req_uuid = (char *) malloc((req_uuid_len + 1) * sizeof(char));
        strcpy(ota_task_data.req_uuid, req_uuid);
        ota_task_data.ota_url = (char *) malloc((ota_url_len + 1) * sizeof(char));
        strcpy(ota_task_data.ota_url, ota_url);
        TaskHandle_t ota_task_handle = NULL;
        BaseType_t ota_task_ret = xTaskCreate(ota_task_code, OTA_UPDATE_TASK_NAME, 4096,
            &ota_task_data, tskIDLE_PRIORITY, &ota_task_handle);
        if (ota_task_ret != pdPASS) {
            ESP_LOGE(TAG, "OTA update task creation failed.");
            pub_ota_event(
                edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INTERNAL, "");
            esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
            return EDGEHOG_ERR_TASK_CREATE;
        }
    } else if (strcmp("Cancel", ota_operation) == 0) {
        // Verify that the update is already in progress
        TaskHandle_t ota_task = xTaskGetHandle(OTA_UPDATE_TASK_NAME);
        if (!ota_task) {
            pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0,
                EDGEHOG_ERR_OTA_INVALID_REQUEST,
                "Unable to cancel OTA update request, no OTA update running.");
            return EDGEHOG_ERR_OTA_INVALID_REQUEST;
        }
        // Open the NVS and fetch the curent update UUID
        nvs_handle_t handle_nvs;
        if (edgehog_device_nvs_open(edgehog_dev, OTA_NAMESPACE, &handle_nvs) != ESP_OK) {
            ESP_LOGE(TAG, "Error opening the NVS OTA update namespace.");
            pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INTERNAL,
                "Unable to cancel OTA update request, NVS error.");
            return EDGEHOG_ERR_OTA_INTERNAL;
        }
        char current_uuid[ASTARTE_UUID_LEN];
        size_t current_uuid_size = ASTARTE_UUID_LEN;
        if (nvs_get_str(handle_nvs, OTA_REQUEST_ID_KEY, current_uuid, &current_uuid_size)
            != ESP_OK) {
            ESP_LOGE(TAG, "Error fetching the OTA update request UUID from NVS.");
            pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INTERNAL,
                "Unable to cancel OTA update request, NVS error.");
            return EDGEHOG_ERR_OTA_INTERNAL;
        }
        // Verify that the request UUID matches the in progress ota update UUID
        if (strcmp(current_uuid, req_uuid) != 0) {
            pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0,
                EDGEHOG_ERR_OTA_INVALID_REQUEST,
                "Unable to cancel OTA update request, not the same UUID.");
            return EDGEHOG_ERR_OTA_INVALID_REQUEST;
        }
        xTaskNotify(ota_task, 0, eNoAction);
    } else {
        pub_ota_event(
            edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INVALID_REQUEST, "");
        return EDGEHOG_ERR_OTA_INVALID_REQUEST;
    }
    return EDGEHOG_OK;
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static void ota_task_code(void *pvParameters)
{
    if (!pvParameters) {
        return;
    }
    ota_task_data_t *task_data = (ota_task_data_t *) pvParameters;
    edgehog_device_handle_t edgehog_dev = ((ota_task_data_t *) pvParameters)->edgehog_dev;
    const char *req_uuid = ((ota_task_data_t *) pvParameters)->req_uuid;
    nvs_handle_t handle_nvs;

    // Step 1 acknowledge the valid update request and notify the start of the download operation.

    pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_ACKNOWLEDGED, 0, EDGEHOG_OK, "");

    // Step 2 Open NVS namespace for the OTA update

    ESP_LOGI(TAG, "OTA INIT");
    esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_INIT_EVENT, NULL, 0, 0);

    esp_err_t esp_err = edgehog_device_nvs_open(edgehog_dev, OTA_NAMESPACE, &handle_nvs);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening the NVS OTA update namespace.");
        ESP_LOGW(TAG, "OTA FAILED");
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_NVS, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
        goto selfdestruct;
    }

    // Step 3 perform the OTA update

    ESP_LOGI(TAG, "DOWNLOAD_AND_DEPLOY");
    nvs_set_u8(handle_nvs, OTA_STATE_KEY, OTA_STATE_IN_PROGRESS);
    nvs_commit(handle_nvs);

    edgehog_err_t edgehog_err = perform_ota(task_data, &handle_nvs);
    if (edgehog_err == EDGEHOG_OK) {
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_DEPLOYING, 0, EDGEHOG_OK, "");
        ESP_LOGI(TAG, "OTA PREPARE REBOOT");
        nvs_set_u8(handle_nvs, OTA_STATE_KEY, OTA_STATE_REBOOT);
        nvs_commit(handle_nvs);
        const esp_partition_t *partition_info = (esp_partition_t *) esp_ota_get_running_partition();
        if (!partition_info) {
            ESP_LOGW(TAG, "OTA FAILED");
            pub_ota_event(
                edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, EDGEHOG_ERR_OTA_INTERNAL, "");
            esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
            goto selfdestruct;
        }
        nvs_set_u32(handle_nvs, OTA_PARTITION_ADDR_KEY, partition_info->address);
        nvs_commit(handle_nvs);
        nvs_close(handle_nvs);
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_DEPLOYED, 0, EDGEHOG_OK, "");
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_REBOOTING, 0, EDGEHOG_OK, "");
        ESP_LOGI(TAG, "Device restart in 5 seconds");
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Device restart now");
        esp_restart();
    } else {
        ESP_LOGW(TAG, "OTA FAILED");
        pub_ota_event(edgehog_dev, req_uuid, OTA_EVENT_FAILURE, 0, edgehog_err, "");
        esp_event_post(EDGEHOG_EVENTS, EDGEHOG_OTA_FAILED_EVENT, NULL, 0, 0);
        nvs_set_u8(handle_nvs, OTA_STATE_KEY, OTA_STATE_IDLE);
        nvs_commit(handle_nvs);
        nvs_close(handle_nvs);
    }

selfdestruct:
    free(((ota_task_data_t *) pvParameters)->req_uuid);
    free(((ota_task_data_t *) pvParameters)->ota_url);

    vTaskDelete(NULL);
}

static edgehog_err_t perform_ota(ota_task_data_t *task_data, nvs_handle_t *handle_nvs)
{
    esp_err_t esp_err;
    edgehog_err_t edgehog_err;

    // Step 1 set the request ID to the received uuid in NVS

    esp_err = nvs_set_str(*handle_nvs, OTA_REQUEST_ID_KEY, task_data->req_uuid);
    nvs_commit(*handle_nvs);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to write OTA req_uuid into NVS, OTA canceled");
        nvs_close(*handle_nvs);
        return EDGEHOG_ERR_NVS;
    }

    // Step 2 attempt OTA operation for MAX_OTA_RETRY tries

    for (uint8_t update_attempts = 0; update_attempts < MAX_OTA_RETRY; update_attempts++) {
        pub_ota_event(
            task_data->edgehog_dev, task_data->req_uuid, OTA_EVENT_DOWNLOADING, 0, EDGEHOG_OK, "");
        edgehog_err = perform_ota_attempt(task_data);
        if (edgehog_err == EDGEHOG_OK || edgehog_err == EDGEHOG_ERR_OTA_CANCELED) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(update_attempts * 2000));
        pub_ota_event(
            task_data->edgehog_dev, task_data->req_uuid, OTA_EVENT_ERROR, 0, edgehog_err, "");
        ESP_LOGW(TAG, "! OTA FAILED, ATTEMPT #%d !", update_attempts);
    }

    // Step 3 check one last time if operation has been canceled

    if (edgehog_err == EDGEHOG_OK
        && pdTRUE == xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, pdMS_TO_TICKS(0u))) {
        edgehog_err = EDGEHOG_ERR_OTA_CANCELED;
    }
    return edgehog_err;
}

static edgehog_err_t perform_ota_attempt(ota_task_data_t *task_data)
{
    esp_https_ota_handle_t https_ota_handle = NULL;

    // Step 1 begin OTA update over HTTPS

    esp_http_client_config_t http_config = {
        .url = task_data->ota_url,
        .timeout_ms = OTA_REQ_TIMEOUT_MS,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    esp_err_t ota_begin_err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (ota_begin_err != ESP_OK || !https_ota_handle) {
        return EDGEHOG_ERR_OTA_INTERNAL;
    }

    // Step 2 wait for OTA update to terminate

    esp_err_t ota_perform_err;
    int image_size = esp_https_ota_get_image_size(https_ota_handle);
    int read_perc;
    int read_perc_rounded;
    int last_perc_sent = 0;

    while (1) {
        if (pdFALSE == xTaskNotifyWait(ULONG_MAX, ULONG_MAX, NULL, pdMS_TO_TICKS(0u))) {
            ota_perform_err = esp_https_ota_perform(https_ota_handle);
            if (ota_perform_err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                float read_size = (float) esp_https_ota_get_image_len_read(https_ota_handle);
                read_perc = (int) (100 * read_size / image_size);
                read_perc_rounded = read_perc - (read_perc % OTA_PROGRESS_PERC_ROUNDING_STEP);
                if (read_perc_rounded != last_perc_sent) {
                    pub_ota_event(task_data->edgehog_dev, task_data->req_uuid,
                        OTA_EVENT_DOWNLOADING, read_perc_rounded, EDGEHOG_OK, "");
                    ESP_LOGI(TAG, "Read perc: %d", read_perc_rounded);
                    last_perc_sent = read_perc_rounded;
                }
            } else {
                break;
            }
        } else {
            esp_https_ota_abort(https_ota_handle);
            ESP_LOGD(TAG, "Update canceled.");
            return EDGEHOG_ERR_OTA_CANCELED;
        }
    }

    // Step 3 finish the OTA update

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        ESP_LOGD(TAG, "Complete data was not received.");
        return EDGEHOG_ERR_NETWORK;
    }
    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    // If there was an error in esp_https_ota_perform(), then it is given more precedence than and
    // error in esp_https_ota_finish().
    if (ota_perform_err == ESP_ERR_INVALID_VERSION
        || ota_perform_err == ESP_ERR_OTA_VALIDATE_FAILED) {
        ESP_LOGD(TAG, "Image validation failed, image is corrupted");
        return EDGEHOG_ERR_OTA_INVALID_IMAGE;
    } else if (ota_perform_err != ESP_OK) {
        ESP_LOGD(TAG, "Update failed 0x%x", ota_perform_err);
        return EDGEHOG_ERR_OTA_INTERNAL;
    }
    if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
        ESP_LOGD(TAG, "Image validation failed, image is corrupted");
        return EDGEHOG_ERR_OTA_INVALID_IMAGE;
    }
    if (ota_finish_err != ESP_OK) {
        ESP_LOGD(TAG, "Update failed 0x%x", ota_finish_err);
        return EDGEHOG_ERR_OTA_INTERNAL;
    }
    return EDGEHOG_OK;
}

static void pub_ota_event(edgehog_device_handle_t edgehog_dev, const char *request_uuid,
    ota_event_t event, int32_t status_progress, edgehog_err_t error, const char *message)
{
    const char *status;
    const char *status_code;

    switch (event) {
        case OTA_EVENT_ACKNOWLEDGED:
            status = "Acknowledged";
            break;
        case OTA_EVENT_DOWNLOADING:
            status = "Downloading";
            break;
        case OTA_EVENT_DEPLOYING:
            status = "Deploying";
            break;
        case OTA_EVENT_DEPLOYED:
            status = "Deployed";
            break;
        case OTA_EVENT_REBOOTING:
            status = "Rebooting";
            break;
        case OTA_EVENT_SUCCESS:
            status = "Success";
            break;
        case OTA_EVENT_FAILURE:
            status = "Failure";
            break;
        default: // OTA_EVENT_ERROR
            status = "Error";
            break;
    }

    switch (error) {
        case EDGEHOG_OK:
            status_code = "";
            break;
        case EDGEHOG_ERR_OTA_INVALID_REQUEST:
            status_code = "InvalidRequest";
            break;
        case EDGEHOG_ERR_OTA_ALREADY_IN_PROGRESS:
            status_code = "UpdateAlreadyInProgress";
            break;
        case EDGEHOG_ERR_NETWORK:
            status_code = "ErrorNetwork";
            break;
        case EDGEHOG_ERR_NVS:
            status_code = "IOError";
            break;
        case EDGEHOG_ERR_OTA_INVALID_IMAGE:
            status_code = "InvalidBaseImage";
            break;
        case EDGEHOG_ERR_OTA_SYSTEM_ROLLBACK:
            status_code = "SystemRollback";
            break;
        case EDGEHOG_ERR_OTA_CANCELED:
            status_code = "Canceled";
            break;
        default: // EDGEHOG_ERR_OTA_INTERNAL
            status_code = "InternalError";
            break;
    }

    astarte_bson_serializer_handle_t bs = astarte_bson_serializer_new();
    astarte_bson_serializer_append_string(bs, "requestUUID", request_uuid);
    astarte_bson_serializer_append_string(bs, "status", status);
    astarte_bson_serializer_append_int32(bs, "statusProgress", status_progress);
    astarte_bson_serializer_append_string(bs, "statusCode", status_code);
    astarte_bson_serializer_append_string(bs, "message", message);
    astarte_bson_serializer_append_end_of_document(bs);

    int doc_len;
    const void *doc = astarte_bson_serializer_get_document(bs, &doc_len);
    astarte_device_stream_aggregate(
        edgehog_dev->astarte_device, ota_event_interface.name, "/event", doc, 0);
    astarte_bson_serializer_destroy(bs);
}

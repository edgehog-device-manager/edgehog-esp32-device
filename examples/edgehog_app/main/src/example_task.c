/*
 * This file is part of Edgehog.
 *
 * Copyright 2023 SECO Mind Srl
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

#include "example_task.h"

#include <astarte_credentials.h>
#include <esp_log.h>

#include "astarte.h"
#include "astarte_device.h"
#include "edgehog_device.h"
#include "edgehog_event.h"

/************************************************
 * Constants and defines
 ***********************************************/

#define TAG "EXAMPLE_TASK"

/************************************************
 * Static variables declarations
 ***********************************************/

static edgehog_device_handle_t edgehog_device;

/************************************************
 * Static functions declaration
 ***********************************************/

/**
 * @brief Initialize an Astarte device.
 *
 * @param notify_handle Task to be notified when the device gets connected.
 * @return The Astarte device instance handle.
 */
static astarte_device_handle_t astarte_device_sdk_init(TaskHandle_t notify_handle);
/**
 * @brief Initialize an Edgehog device and starts the associated astarte device.
 *
 * @param arg Extra args.
 *
 * @return The Edgehog device instance handle.
 */
static edgehog_device_handle_t edgehog_device_init(astarte_device_handle_t astarte_handle);
/**
 * @brief Handler for edgehog events.
 *
 * @param arg Extra args.
 * @param event_base Edgehog event base.
 * @param event_id Event identifier.
 * @param event_data Event data.
 */
static void edgehog_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
/**
 * @brief Handler for astarte connection events.
 *
 * @param event Astarte device connection event pointer.
 */
static void astarte_connection_events_handler(astarte_device_connection_event_t *event);
/**
 * @brief Handler for astarte data event.
 *
 * @param event Astarte device data event pointer.
 */
static void astarte_data_events_handler(astarte_device_data_event_t *event);
/**
 * @brief Handler for astarte disconnection events.
 *
 * @param event Astarte device disconnection event pointer.
 */
static void astarte_disconnection_events_handler(astarte_device_disconnection_event_t *event);

/************************************************
 * Global functions definition
 ***********************************************/

void edgehog_example_task(void *ctx)
{
    astarte_device_handle_t astarte_handle = astarte_device_sdk_init(xTaskGetCurrentTaskHandle());
    assert(astarte_handle);
    edgehog_device = edgehog_device_init(astarte_handle);
    assert(edgehog_device);

    // Start the Astarte device
    if (astarte_device_start(astarte_handle) != ASTARTE_OK) {
        ESP_LOGE(TAG, "Failed starting the Astarte device.");
        vTaskDelete(NULL);
    }

    // Wait until the Astarte device is connected and the current task gets notified
    (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Astarte device started and connected, starting edgehog device...");

    // Start the Edgehog device
    edgehog_err_t edgehog_err = edgehog_device_start(edgehog_device);
    if (edgehog_err != EDGEHOG_OK) {
        ESP_LOGE(TAG, "Failed starting the Edgehog device");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Edgehog device started");

    while (true) {
        // In this example this task is running with the lower priority and does not starve
        // the mcu thanks to time slicing.
        // If in your code you want to run this task with a higher priority make sure not to
        // starve the mcu by placing vTaskDelay() in here.
    }
}

/************************************************
 * Static functions definitions
 ***********************************************/

static astarte_device_handle_t astarte_device_sdk_init(TaskHandle_t notify_handle)
{
    // Set up storage for the Astarte device
    if (astarte_credentials_use_nvs_storage(ASTARTE_PARTITION_NAME) != ASTARTE_OK) {
        ESP_LOGE(TAG, "Failed setting NVS as storage defaults.");
        return NULL;
    }

    // Initialize the credentials for the Astarte device
    if (astarte_credentials_init() != ASTARTE_OK) {
        ESP_LOGE(TAG, "Failed initializing the Astarte credentials.");
        return NULL;
    }

    astarte_device_config_t astarte_device_cfg = {
        .connection_event_callback = astarte_connection_events_handler,
        .disconnection_event_callback = astarte_disconnection_events_handler,
        .data_event_callback = astarte_data_events_handler,
        .callbacks_user_data = notify_handle,
        .credentials_secret = CONFIG_CREDENTIALS_SECRET,
        .hwid = CONFIG_DEVICE_ID,
    };

    astarte_device_handle_t astarte_device = astarte_device_init(&astarte_device_cfg);
    if (!astarte_device) {
        ESP_LOGE(TAG, "Failed to init astarte device");
        return NULL;
    }

    return astarte_device;
}

static edgehog_device_handle_t edgehog_device_init(astarte_device_handle_t astarte_handle)
{
    // Setup the event handler for the Edgehog device
    esp_err_t esp_err = esp_event_handler_instance_register(
        EDGEHOG_EVENTS, ESP_EVENT_ANY_ID, edgehog_event_handler, NULL, NULL);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed registering the Edgehog event handler.");
        return NULL;
    }

    // Initialize the Edgehog device
    edgehog_device_telemetry_config_t telemetry_config = {
        .type = EDGEHOG_TELEMETRY_SYSTEM_STATUS,
        .period_seconds = 3600,
    };
    edgehog_device_config_t edgehog_conf = {
        .astarte_device = astarte_handle,
        .partition_label = EDGEHOG_PARTITION_NAME,
        .telemetry_config = &telemetry_config,
        .telemetry_config_len = 1,
    };
    edgehog_device_handle_t edgehog_device = edgehog_device_new(&edgehog_conf);
    if (!edgehog_device) {
        ESP_LOGE(TAG, "Failed initializing the Edgehog device.");
        return NULL;
    }

    // Declare serial and part number for the example Edgehog device
    esp_err = edgehog_device_set_system_serial_number(edgehog_device, "edgehog_example_sn_1");
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed setting the system serial number for Edgehog.");
        return NULL;
    }
    esp_err
        = edgehog_device_set_system_part_number(edgehog_device, "edgehog_example_part_number_1");
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed setting the system part number for Edgehog.");
        return NULL;
    }
    return edgehog_device;
}

static void edgehog_event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == EDGEHOG_EVENTS) {
        switch (event_id) {
            case EDGEHOG_INVALID_EVENT:
                ESP_LOGI(TAG, "EDGEHOG EVENT RECEIVED: EDGEHOG_INVALID_EVENT.");
                break;
            case EDGEHOG_OTA_INIT_EVENT:
                ESP_LOGI(TAG, "EDGEHOG EVENT RECEIVED: EDGEHOG_OTA_INIT_EVENT.");
                break;
            case EDGEHOG_OTA_FAILED_EVENT:
                ESP_LOGI(TAG, "EDGEHOG EVENT RECEIVED: EDGEHOG_OTA_FAILED_EVENT.");
                break;
            case EDGEHOG_OTA_SUCCESS_EVENT:
                ESP_LOGI(TAG, "EDGEHOG EVENT RECEIVED: EDGEHOG_OTA_SUCCESS_EVENT.");
                break;
            default:
                ESP_LOGI(TAG, "EDGEHOG EVENT RECEIVED: Event not recognized.");
                break;
        }
    }
}

static void astarte_connection_events_handler(astarte_device_connection_event_t *event)
{
    ESP_LOGI(TAG, "Astarte device connected, session_present: %d", event->session_present);
    // Unlocking the example task
    xTaskNotifyGive(event->user_data);
}

static void astarte_data_events_handler(astarte_device_data_event_t *event)
{
    ESP_LOGI(TAG, "Got Astarte data event, interface_name: %s, path: %s, bson_type: %d",
        event->interface_name, event->path, event->bson_element.type);
    edgehog_device_astarte_event_handler(edgehog_device, event);
}

static void astarte_disconnection_events_handler(astarte_device_disconnection_event_t *event)
{
    ESP_LOGI(TAG, "Astarte device disconnected");
}

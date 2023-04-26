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

#include "edgehog_led.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <astarte_bson.h>
#include <astarte_bson_types.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#define STACK_SIZE 2048

static const char *TAG = "EDGEHOG_LED_BEHAVIOR";

const astarte_interface_t led_request_interface = { .name = "io.edgehog.devicemanager.LedBehavior",
    .major_version = 0,
    .minor_version = 1,
    .ownership = OWNERSHIP_SERVER,
    .type = TYPE_DATASTREAM };

typedef struct
{
    led_behavior_t behavior;
    int duration_in_sec;
    led_behavior_t default_behavior;
    bool terminated;
} led_behavior_config_t;

struct edgehog_led_behavior_manager_t
{
    TaskHandle_t task_handle;
    led_behavior_config_t *current_config;
    led_behavior_t default_behavior;
};

edgehog_led_behavior_manager_handle_t edgehog_led_behavior_manager_new()
{

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pin to set
    io_conf.pin_bit_mask = (1ULL << CONFIG_INDICATOR_GPIO);
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);
#else
    // Set the pad as GPIO
    gpio_pad_select_gpio(CONFIG_INDICATOR_GPIO);
    // Set LED GPIO as output
    gpio_set_direction(CONFIG_INDICATOR_GPIO, GPIO_MODE_OUTPUT);
#endif

    edgehog_led_behavior_manager_handle_t led_behavior_handle
        = calloc(1, sizeof(struct edgehog_led_behavior_manager_t));
    if (!led_behavior_handle) {
        ESP_LOGE(TAG, "Unable to allocate memory for led behavior manager handle");
        return NULL;
    }
    led_behavior_handle->default_behavior = OFF;
    return led_behavior_handle;
}

static void blinkTaskCode(void *pvParameters)
{
    led_behavior_config_t *params = (led_behavior_config_t *) pvParameters;
    int64_t start_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Started behavior %d", params->behavior);
    while ((esp_timer_get_time() - start_time) < (params->duration_in_sec * 1000000)) {
        switch (params->behavior) {
            case BLINK:
                gpio_set_level(CONFIG_INDICATOR_GPIO, ON);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                gpio_set_level(CONFIG_INDICATOR_GPIO, OFF);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            case DOUBLE_BLINK:
                gpio_set_level(CONFIG_INDICATOR_GPIO, ON);
                vTaskDelay(300 / portTICK_PERIOD_MS);
                gpio_set_level(CONFIG_INDICATOR_GPIO, OFF);
                vTaskDelay(200 / portTICK_PERIOD_MS);
                gpio_set_level(CONFIG_INDICATOR_GPIO, ON);
                vTaskDelay(300 / portTICK_PERIOD_MS);
                gpio_set_level(CONFIG_INDICATOR_GPIO, OFF);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            case SLOW_BLINK:
                gpio_set_level(CONFIG_INDICATOR_GPIO, ON);
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                gpio_set_level(CONFIG_INDICATOR_GPIO, OFF);
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                break;
            default:
                ESP_LOGE(TAG, "Unexpected Led behavior %d", params->behavior);
                goto loop_end;
        }
    }

loop_end:
    params->terminated = true;
    gpio_set_level(CONFIG_INDICATOR_GPIO, params->default_behavior);
    ESP_LOGI(TAG, "Task ended gracefully by timeout");
    vTaskDelete(NULL);
    free(params);
}

static edgehog_err_t set_led_behavior(
    edgehog_led_behavior_manager_handle_t led_manager, led_behavior_t behavior, int duration_in_sec)
{
    if (led_manager->current_config) {
        if (!led_manager->current_config->terminated && led_manager->task_handle) {
            ESP_LOGI(TAG, "New behavior received before previous ended. Previous Task killed");
            vTaskDelete(led_manager->task_handle);
        }
        free(led_manager->current_config);
        led_manager->current_config = NULL;
    }

    led_manager->current_config = malloc(sizeof(led_behavior_config_t));
    if (!led_manager->current_config) {
        ESP_LOGE(TAG, "Unable to allocate memory for led behavior config");
        return EDGEHOG_ERR;
    }
    led_manager->current_config->behavior = behavior;
    led_manager->current_config->duration_in_sec = duration_in_sec;
    led_manager->current_config->default_behavior = led_manager->default_behavior;
    led_manager->current_config->terminated = false;

    BaseType_t ret = xTaskCreate(blinkTaskCode, "NAME", STACK_SIZE, led_manager->current_config,
        tskIDLE_PRIORITY, &led_manager->task_handle);

    if (ret != pdPASS) {
        free(led_manager->current_config);
        return EDGEHOG_ERR_TASK_CREATE;
    }

    ESP_LOGI(TAG, "Task handle: %p", led_manager->task_handle);
    return EDGEHOG_OK;
}

void edgehog_led_behavior_set_default(
    edgehog_led_behavior_manager_handle_t led_manager, led_behavior_t default_behavior)
{
    if (default_behavior != ON && default_behavior != OFF) {
        ESP_LOGE(TAG, "Only ON and OFF behavior are supported as default");
    }

    led_manager->default_behavior = default_behavior;
    gpio_set_level(CONFIG_INDICATOR_GPIO, default_behavior);
}

edgehog_err_t edgehog_led_behavior_event(
    edgehog_led_behavior_manager_handle_t led_manager, astarte_device_data_event_t *event_request)
{
    EDGEHOG_VALIDATE_INCOMING_DATA(TAG, event_request, "/indicator/behavior", BSON_TYPE_STRING);

    uint32_t str_value_len;
    const char *request_behavior
        = astarte_bson_value_to_string(event_request->bson_value, &str_value_len);

    if (strcmp(request_behavior, "Blink60Seconds") == 0) {
        return set_led_behavior(led_manager, BLINK, 60);
    } else if (strcmp(request_behavior, "DoubleBlink60Seconds") == 0) {
        return set_led_behavior(led_manager, DOUBLE_BLINK, 60);
    } else if (strcmp(request_behavior, "SlowBlink60Seconds") == 0) {
        return set_led_behavior(led_manager, SLOW_BLINK, 60);
    }

    ESP_LOGE(TAG, "Unable to handle led behavior request, behavior not supported");
    return EDGEHOG_ERR;
}

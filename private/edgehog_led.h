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

#ifndef EDGEHOG_LED_H
#define EDGEHOG_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "astarte_device.h"
#include <edgehog.h>

/**
 * @brief Supported LED behaviors
 *
 * @details Enum defining the possible behaviors supported. The ON/OFF statuses assume the external
 * LED is connected to ground through a resistive load.
 */
typedef enum
{
    OFF = 0, /**< Always OFF - Only for Default behavior*/
    ON, /**< Always ON - Only for Default behavior*/
    BLINK, /**< Blinking behavior 1 sec. ON 1 sec, OFF*/
    DOUBLE_BLINK, /**< Two small blink and 1 sec. OFF*/
    SLOW_BLINK /**< Slow blinking behavior. 2 sec. ON 2 sec. OFF*/
} led_behavior_t;

struct edgehog_led_behavior_manager_t;

typedef struct edgehog_led_behavior_manager_t *edgehog_led_behavior_manager_handle_t;

extern const astarte_interface_t led_request_interface;

/**
 * @brief Create a new LED behavior manager
 *
 * @details Create and initialize a new LED behavior manager with OFF default behavior
 *
 * @return an handle for the led behavior manager
 */
edgehog_led_behavior_manager_handle_t edgehog_led_behavior_manager_new();

/**
 * @brief Set the default led behavior
 *
 * @details Sets a default behavior for the given LED. Only ON or OFF are currently supported
 *
 * @param led_manager Handle to the LED that has to be set
 * @param default_behavior The default behavior between ON or OFF
 */
void edgehog_led_behavior_set_default(
    edgehog_led_behavior_manager_handle_t led_manager, led_behavior_t default_behavior);

/**
 * @brief Handle function for a `io.edgehog.devicemanager.LedBehavior` message
 *
 * @details This function receives a LedBehavior message and sets the behavior described
 * in the message for the number of seconds requested
 *
 * @param led_manager handle to the LED that has to be set
 * @param event_request The request message
 * @return EDGEHOG_OK if the message is correctly handled, EDGEHOG_ERR_TASK_CREATE otherwise.
 */
edgehog_err_t edgehog_led_behavior_event(
    edgehog_led_behavior_manager_handle_t led_manager, astarte_device_data_event_t *event_request);

#endif // EDGEHOG_LED_H

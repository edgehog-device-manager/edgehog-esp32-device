/*
 * This file is part of Edgehog.
 *
 * Copyright 2021,2022 SECO Mind Srl
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

/**
 * @file edgehog_battery_status.h
 * @brief Edgehog device battery status API.
 */

#ifndef EDGEHOG_BATTERY_STATUS_H
#define EDGEHOG_BATTERY_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "edgehog_device.h"

// clang-format off

/**
 * @brief Edgehog Battery state codes.
 */
typedef enum
{
    BATTERY_INVALID = 0, /**< The battery state for the device is invalid. */
    BATTERY_IDLE, /**< The device is plugged into power and the battery is 100% charged. */
    BATTERY_CHARGING, /**< The device is plugged into power and the battery is less than 100% charged. */
    BATTERY_DISCHARGING, /**< The device is not plugged into power; the battery is discharging. */
    BATTERY_IDLE_OR_CHARGING, /**< The battery state for the device cannot be distinguished between "Idle" and "Charging". */
    BATTERY_FAILURE, /**< A generic failure occurred. */
    BATTERY_REMOVED, /**< Battery removed from the device */
    BATTERY_UNKNOWN /**< The battery state for the device cannot be determined. */
} edgehog_battery_state;

typedef struct {
    const char *battery_slot; /**< Battery slot name. */
    double level_percentage; /**< Charge level in [0.0%-100.0%] range, such as 89.0%. */
    double level_absolute_error; /**< The level measurement absolute error in [0.0-100.0] range. */
    edgehog_battery_state battery_state; /**< Any value of edgehog_battery_state such as `BATTERY_CHARGING`. */
} edgehog_battery_status_t;

// clang-format on

/**
 * @brief Update battery status info.
 *
 * @details This function updates battery status info. This function does not immediately publish
 * the update.
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param battery_status A battery status structure that contains current battery status. It can be
 * safely allocated on the stack, a copy of it is automatically stored.
 */
void edgehog_battery_status_update(
    edgehog_device_handle_t edgehog_device, const edgehog_battery_status_t *battery_status);

/**
 * @brief Publish battery status info.
 *
 * @details This function publishes to Astarte all available battery status updates.
 *
 * @param edgehog_device A valid Edgehog device handle.
 */
void edgehog_battery_status_publish(edgehog_device_handle_t edgehog_device);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_BATTERY_STATUS_H

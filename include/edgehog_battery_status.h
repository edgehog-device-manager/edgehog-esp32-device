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

#ifndef EDGEHOG_BATTERY_STATUS_H
#define EDGEHOG_BATTERY_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "astarte_device.h"

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

// clang-format on

extern const astarte_interface_t battery_status_interface;

/**
 * @brief publish Battery status info.
 *
 * @details This function publishes battery status info
 * to Astarte.
 *
 * @param astarte_device A valid Astarte device handle.
 * @param battery_slot Battery slot name.
 * @param level_percentage Charge level in [0.0%-100.0%] range, such as 89.0%.
 * @param level_absolute_error The level measurement absolute error in [0.0-100.0] range
 * @param state Any value of edgehog_battery_state such as `BATTERY_CHARGING`
 *
 */
void edgehog_battery_status_publish(astarte_device_handle_t astarte_device,
    const char *battery_slot, double level_percentage, double level_absolute_error,
    edgehog_battery_state state);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_BATTERY_STATUS_H

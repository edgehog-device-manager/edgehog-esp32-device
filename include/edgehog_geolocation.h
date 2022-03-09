/*
 * This file is part of Edgehog.
 *
 * Copyright 2022 SECO Mind Srl
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

#ifndef EDGEHOG_GEOLOCATION_H
#define EDGEHOG_GEOLOCATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "edgehog_device.h"

/**
 * @brief Edgehog geolocation Data struct
 */
typedef struct
{
    const char *id; /* GPS receiver identifier */
    double longitude; /* Sampled longitude value. */
    double latitude; /* Sampled latitude value. */
    double accuracy; /* Sampled accuracy of the latitude and longitude properties. */
    double altitude; /* Sampled altitude value. */
    double altitude_accuracy; /* Sampled accuracy of the altitude property. */
    double heading; /*Sampled value of the direction towards which the device is facing. */
    double speed; /*Sampled value representing the velocity of the device.*/
} edgehog_geolocation_data_t;

/**
 * @brief Publish geolocation data.
 *
 * @details This function publishes to Astarte all available geolocation data updates.
 *
 * @param edgehog_device A valid Edgehog device handle.
 */
void edgehog_geolocation_publish(edgehog_device_handle_t edgehog_device);

/**
 * @brief Update geolocation info
 *
 * @details This function updates geolocation data. This function does not immediately publish
 * the update.
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param battery_status A geolocation data structure that contains current geolocation. It can be
 * safely allocated on the stack, a copy of it is automatically stored.
 */
void edgehog_geolocation_update(
    edgehog_device_handle_t edgehog_device, edgehog_geolocation_data_t *update);
#endif // EDGEHOG_GEOLOCATION_H

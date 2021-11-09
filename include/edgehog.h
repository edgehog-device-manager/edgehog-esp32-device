/*
 * This file is part of Edgehog.
 *
 * Copyright 2021 SECO Mind
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

#ifndef EDGEHOG_H
#define EDGEHOG_H

typedef struct edgehog_device_t *edgehog_device_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

#include <astarte_device.h>
/**
 * @brief create Edgehog device handle.
 *
 * @details This function creates an Edgehog device handle. It must be called before anything else.
 * @param astarte_device A valid Astarte device handle.
 * @return The handle to the device, NULL if an error occurred.
 */

edgehog_device_handle_t edgehog_new(astarte_device_handle_t astarte_device);

/**
 * @brief destroy Edgehog device.
 *
 * @details This function destroys the device, freeing all its resources.
 * @param edgehog_device A valid Edgehog device handle.
 */
void edgehog_device_destroy(edgehog_device_handle_t edgehog_device);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_H

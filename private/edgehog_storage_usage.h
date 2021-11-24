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

#ifndef EDGEHOG_STORAGE_USAGE_H
#define EDGEHOG_STORAGE_USAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "astarte_device.h"

extern const astarte_interface_t storage_usage_interface;

/**
 * @brief publish Storage Usage info.
 *
 * @details This function fetches and publishes storage usage info
 * on Astarte.
 *
 * @param astarte_device A valid Astarte device handle.
 */

void edgehog_storage_usage_publish(astarte_device_handle_t astarte_device);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_STORAGE_USAGE_H

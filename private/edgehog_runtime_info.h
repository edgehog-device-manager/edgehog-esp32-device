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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EDGEHOG_RUNTIME_INFO_H
#define EDGEHOG_RUNTIME_INFO_H

#include "edgehog_device.h"

extern const astarte_interface_t runtime_info_interface;

/**
 * @brief Publish runtime info
 *
 * @details This function fetches and publishes the edgehog runtime info to Astarte
 *
 * @param edgehog_device A valid Edgehog device handle.
 */
void edgehog_runtime_info_publish(edgehog_device_handle_t edgehog_device);

#endif // EDGEHOG_RUNTIME_INFO_H

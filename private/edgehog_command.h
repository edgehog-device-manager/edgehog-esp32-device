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

#ifndef EDGEHOG_COMMAND_H
#define EDGEHOG_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "astarte_device.h"
#include "edgehog.h"

extern const astarte_interface_t commands_interface;

/**
 * @brief receive Edgehog device commands.
 *
 * @details This function receives a command request from Astarte.
 *
 * @param event_request A valid Astarte device data event.
 *
 * @return EDGEHOG_OK if the command event is handled successfully, an edgehog_err_t otherwise.
 */

edgehog_err_t edgehog_command_event(astarte_device_data_event_t *event_request);

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_COMMAND_H

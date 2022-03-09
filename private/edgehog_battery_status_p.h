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

#ifndef EDGEHOG_BATTERY_STATUS_P_H
#define EDGEHOG_BATTERY_STATUS_P_H

#include "edgehog_device.h"

#include <astarte_list.h>

/**
 * Private API.
 */

extern const astarte_interface_t battery_status_interface;

void edgehog_battery_status_delete_list(struct astarte_list_head_t *battery_list);

#endif // EDGEHOG_BATTERY_STATUS_P_H

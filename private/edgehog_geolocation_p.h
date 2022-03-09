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

#ifndef EDGEHOG_GEOLOCATION_P_H
#define EDGEHOG_GEOLOCATION_P_H
#include "edgehog_device.h"
#include <astarte_list.h>

// clang-format on

extern const astarte_interface_t geolocation_interface;

void edgehog_geolocation_delete_list(struct astarte_list_head_t *geolocation_list);

#endif // EDGEHOG_GEOLOCATION_P_H

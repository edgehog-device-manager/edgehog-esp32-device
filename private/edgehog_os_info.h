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

#ifndef EDGEHOG_OS_INFO_H
#define EDGEHOG_OS_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <astarte_device.h>

extern const astarte_interface_t os_info_interface;

void edgehog_device_publish_os_info(astarte_device_handle_t astarte_device);
#endif // EDGEHOG_OS_INFO_H

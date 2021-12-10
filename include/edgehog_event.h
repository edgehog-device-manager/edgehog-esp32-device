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

#ifndef EDGEHOG_EVENT_H
#define EDGEHOG_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(EDGEHOG_EVENTS);

/**
 * @brief Edgehog event codes.
 */
typedef enum
{
    EDGEHOG_INVALID_EVENT = 0, /**< An invalid event. */
    EDGEHOG_OTA_INIT_EVENT, /**< Edgehog OTA routine init. */
    EDGEHOG_OTA_FAILED_EVENT, /**< Edgehog OTA routine failed. */
    EDGEHOG_OTA_SUCCESS_EVENT /**< Edgehog OTA routine successful. */
} edgehog_event;

#ifdef __cplusplus
}
#endif

#endif // EDGEHOG_EVENT_H

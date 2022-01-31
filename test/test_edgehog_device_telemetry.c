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

#include "edgehog_device.h"
#include "edgehog_telemetry.c"
#include <unity.h>

TEST_CASE("edgehog_device_telemetry_1", "create an edgehog_device_telemetry with one telemetry")
{
    edgehog_device_telemetry_config_t telemetry_config
        = { .type = EDGEHOG_TELEMETRY_SYSTEM_STATUS, .period_seconds = 3600 };

    edgehog_telemetry_t *edgehog_telemetry
        = edgehog_telemetry_new(&telemetry_config, 1);
    TEST_ASSERT(edgehog_telemetry)
    TEST_ASSERT(edgehog_telemetry->telemetry_config_len == 1)
    edgehog_telemetry_destroy(edgehog_telemetry);
}


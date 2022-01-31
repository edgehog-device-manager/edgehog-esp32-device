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
#include "unity.h"
#include <astarte_credentials.h>

TEST_CASE("edgehog_device_1", "create an edgehog_device with NULL edgehog_conf")
{
    edgehog_device_handle_t edgehog_device = edgehog_device_new(NULL);
    TEST_ASSERT(!edgehog_device);
}

TEST_CASE("edgehog_device_2", "create an edgehog_device with NULL astarte device in edgehog_conf")
{
    edgehog_device_config_t edgehog_conf = { .astarte_device = NULL, .partition_label = "nvs" };
    edgehog_device_handle_t edgehog_device = edgehog_device_new(&edgehog_conf);
    TEST_ASSERT(!edgehog_device);
}

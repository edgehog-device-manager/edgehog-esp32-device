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
#include "edgehog_device.c"
#include "unity.h"
#include <astarte_credentials.h>
#include "edgehog_device_private.h"

TEST_CASE("edgehog_device_01", "create an edgehog_device with NULL edgehog_conf")
{
    edgehog_device_handle_t edgehog_device = edgehog_device_new(NULL);
    TEST_ASSERT(!edgehog_device);
}

TEST_CASE("edgehog_device_02", "create an edgehog_device with NULL astarte device in edgehog_conf")
{
    edgehog_device_config_t edgehog_conf = { .astarte_device = NULL, .partition_label = "nvs" };
    edgehog_device_handle_t edgehog_device = edgehog_device_new(&edgehog_conf);
    TEST_ASSERT(!edgehog_device);
}

TEST_CASE("edgehog_device_03", "edgehog_nvs_set_str wrong partition returns != ESP_OK")
{
    esp_err_t result = edgehog_nvs_set_str("nvs_bad", "", NULL);
    TEST_ASSERT(result != ESP_OK);
}

TEST_CASE("edgehog_device_04", "edgehog_nvs_get_string returns NULL")
{
    char* value_returned = edgehog_nvs_get_string("nvs", "key2");
    TEST_ASSERT(!value_returned)
}

TEST_CASE("edgehog_device_05", "edgehog_nvs_set_string set value returns ESP_OK")
{
    char *value_expected = "value";
    esp_err_t result = edgehog_nvs_set_str("nvs", "key1", value_expected);
    TEST_ASSERT(result == ESP_OK)
    edgehog_nvs_set_str("nvs", "key1", "");
}

TEST_CASE("edgehog_device_06", "edgehog_nvs_get_string get value returns ESP_OK")
{
    char *value_expected = "value";
    esp_err_t result = edgehog_nvs_set_str("nvs", "key1", value_expected);
    TEST_ASSERT(result == ESP_OK);
    char* value_returned = edgehog_nvs_get_string("nvs", "key1");
    TEST_ASSERT(value_returned)
    TEST_ASSERT(strcmp(value_expected, value_returned) == 0)
    free(value_returned);
}

TEST_CASE("edgehog_device_07", "edgehog_device_get_telemetry_periodic returns publish_device_hardware_info function")
{
    telemetry_periodic telemetry_periodic_expected = publish_device_hardware_info;
    telemetry_periodic telemetry_periodic_fn =  edgehog_device_get_telemetry_periodic(EDGEHOG_TELEMETRY_HW_INFO);
    TEST_ASSERT(telemetry_periodic_fn)
    TEST_ASSERT(telemetry_periodic_fn == telemetry_periodic_expected)
}

TEST_CASE("edgehog_device_08", "edgehog_device_get_telemetry_periodic doesn't publish_device_hardware_info function")
{
    telemetry_periodic telemetry_periodic_expected = scan_wifi_ap;
    telemetry_periodic telemetry_periodic_fn =  edgehog_device_get_telemetry_periodic(EDGEHOG_TELEMETRY_HW_INFO);
    TEST_ASSERT(telemetry_periodic_fn)
    TEST_ASSERT(telemetry_periodic_fn != telemetry_periodic_expected)
}

TEST_CASE("edgehog_device_09", "edgehog_device_get_telemetry_periodic returns NULL")
{
    telemetry_periodic telemetry_periodic_fn =  edgehog_device_get_telemetry_periodic(EDGEHOG_TELEMETRY_INVALID);
    TEST_ASSERT(!telemetry_periodic_fn)
}

TEST_CASE("edgehog_device_10", "edgehog_device_get_telemetry_type returns EDGEHOG_TELEMETRY_INVALID")
{
    telemetry_type_t  telemetry_type = edgehog_device_get_telemetry_type("");
    TEST_ASSERT(telemetry_type == EDGEHOG_TELEMETRY_INVALID)
}

TEST_CASE("edgehog_device_11", "edgehog_device_get_telemetry_type returns EDGEHOG_TELEMETRY_HW_INFO")
{
    telemetry_type_t  telemetry_type = edgehog_device_get_telemetry_type(hardware_info_interface.name);
    TEST_ASSERT(telemetry_type == EDGEHOG_TELEMETRY_HW_INFO)
}

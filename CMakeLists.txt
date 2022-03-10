#
# This file is part of Edgehog.
#
# Copyright 2021,2022 SECO Mind Srl
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#

set(edgehog_srcs "src/edgehog_device.c"
        "src/edgehog_ota.c"
        "src/edgehog_storage_usage.c"
        "src/edgehog_battery_status.c"
        "src/edgehog_command.c"
        "src/edgehog_os_info.c"
        "src/edgehog_base_image.c"
        "src/edgehog_telemetry.c"
        "src/edgehog_runtime_info.c"
        "src/edgehog_cellular_connection.c"
        "src/edgehog_network_interface.c"
        "src/edgehog_geolocation.c")

if (${CONFIG_INDICATOR_GPIO_ENABLE})
    set(edgehog_srcs ${edgehog_srcs} "src/edgehog_led.c")
endif ()

idf_component_register(SRCS "${edgehog_srcs}"
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "private"
        REQUIRES astarte-device-sdk-esp32 nvs_flash app_update esp_https_ota)

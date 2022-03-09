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

#include "edgehog_network_interface.h"
#include "edgehog_device_private.h"
#include <esp_log.h>

static const char *TAG = "EDGEHOG_BATTERY";

const astarte_interface_t netif_interface
    = { .name = "io.edgehog.devicemanager.NetworkInterfaceProperties",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

static const char *edgehog_netif_technology_type_to_string(
    edgehog_netif_technology_type technology_type);

void edgehog_netif_properties_publish(edgehog_device_handle_t edgehog_device,
    const char *iface_name, const char *mac_address, edgehog_netif_technology_type technology_type)
{
    char path[64];
    if (snprintf(path, 64, "/%s/macAddress", iface_name) >= sizeof(path)) {
        ESP_LOGE(TAG, "Unable to publish macAddress for iface %s", iface_name);
        return;
    }
    astarte_err_t ret = astarte_device_set_string_property(
        edgehog_device->astarte_device, netif_interface.name, path, mac_address);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish macAddress for iface %s", iface_name);
        return;
    }
    if (snprintf(path, 64, "/%s/technologyType", iface_name) >= sizeof(path)) {
        ESP_LOGE(TAG, "Unable to publish technology type for iface %s", iface_name);
        return;
    }
    ret = astarte_device_set_string_property(edgehog_device->astarte_device, netif_interface.name,
        path, edgehog_netif_technology_type_to_string(technology_type));
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish technology type for iface %s", iface_name);
        return;
    }
}

static const char *edgehog_netif_technology_type_to_string(
    edgehog_netif_technology_type technology_type)
{
    switch (technology_type) {
        case NETWORK_INTERFACE_TECHNOLOGY_ETHERNET:
            return "Ethernet";
        case NETWORK_INTERFACE_TECHNOLOGY_BLUETOOTH:
            return "Bluetooth";
        case NETWORK_INTERFACE_TECHNOLOGY_CELLULAR:
            return "Cellular";
        case NETWORK_INTERFACE_TECHNOLOGY_WIFI:
            return "WiFi";
        default:
            return "";
    }
}

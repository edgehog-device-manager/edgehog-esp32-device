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

#ifndef EDGEHOG_NETWORK_INTERFACE_H
#define EDGEHOG_NETWORK_INTERFACE_H

#include "edgehog_device.h"

/**
 * @brief Edgehog network interface technology type codes
 */
typedef enum
{
    NETWORK_INTERFACE_TECHNOLOGY_INVALID = 0,
    NETWORK_INTERFACE_TECHNOLOGY_ETHERNET,
    NETWORK_INTERFACE_TECHNOLOGY_BLUETOOTH,
    NETWORK_INTERFACE_TECHNOLOGY_CELLULAR,
    NETWORK_INTERFACE_TECHNOLOGY_WIFI,
} edgehog_netif_technology_type;

extern const astarte_interface_t netif_interface;

/**
 * @brief Publish Network interface properties
 *
 * @details This function publishes  network interface properties to Astarte
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param iface_name The network interface name
 * @param mac_address The network interface mac address
 * @param technology_type The network interface technology type
 */
void edgehog_netif_properties_publish(edgehog_device_handle_t edgehog_device,
    const char *iface_name, const char *mac_address, edgehog_netif_technology_type technology_type);

#endif // EDGEHOG_NETWORK_INTERFACE_H

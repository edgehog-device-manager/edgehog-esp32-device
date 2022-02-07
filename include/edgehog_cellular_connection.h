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

/**
 * @file edgehog_cellular_connection.h
 * @brief Edgehog device cellular connection API.
 */

#ifndef EDGEHOG_CELLULAR_CONNECTION_H
#define EDGEHOG_CELLULAR_CONNECTION_H

#include "edgehog_device.h"

/**
 * @brief Edgehog Registration Status Codes
 */
typedef enum
{
    INVALID_REGISTRATION_STATUS = 0,
    NOT_REGISTERED,
    REGISTERED,
    SEARCHING_OPERATOR,
    REGISTRATION_DENIED,
    UNKNOWN,
    REGISTERED_ROAMING
} edgehog_registration_status;

/**
 * @brief Edgehog Connection Technology codes
 */
typedef enum
{
    INVALID_CONNECTION_TECHNOLOGY = 0,
    GSM,
    GSM_COMPACT,
    UTRAN,
    GSM_WITH_EGPRS,
    UTRAN_WITH_HSDPA,
    UTRAN_WITH_HSUPA,
    UTRAN_WITH_HSDPA_AND_HSUPA,
    E_UTRAN
} edgehog_connection_technology;

extern const astarte_interface_t cellular_connection_status_interface;
extern const astarte_interface_t cellular_connection_properties_interface;

/**
 * @brief Publish Connection status telemetry data
 *
 * @details This function publishes the connection status telemetry data to Astarte
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param modem_id Identifier of the modem which the data belongs to
 * @param carrier Name or code of the network operator
 * @param technology Network technology
 * @param registration_status Connection status on the network
 * @param rssi Connection signal strength
 * @param cell_id Identifier of the network cell
 * @param local_area_code Identifier of the area
 * @param mobile_country_code The mobile country code (MCC) for the device's home network
 * @param mobile_network_code The Mobile Network Code for the device's home network
 */
void edgehog_connection_status_publish(edgehog_device_handle_t edgehog_device, char *modem_id,
    const char *carrier, edgehog_connection_technology technology,
    edgehog_registration_status registration_status, double rssi, int64_t cell_id,
    int local_area_code, int mobile_country_code, int mobile_network_code);

/**
 * @brief Publish connection properties
 *
 * @details This function publishes the GSM/LTE modem info on Astarte
 *
 * @param edgehog_device A valid Edgehog device handle.
 * @param modem_id Identifier of the modem which the data belongs to
 * @param imei Modem unique identifier
 * @param imsi SIM unique identifier
 * @param apn Access point name configured for the modem
 */
void edgehog_connection_properties_publish(edgehog_device_handle_t edgehog_device,
    const char *modem_id, const char *imei, const char *imsi, const char *apn);

#endif // EDGEHOG_CELLULAR_CONNECTION_H

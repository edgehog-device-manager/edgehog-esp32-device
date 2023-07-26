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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "edgehog_cellular_connection.h"
#include "edgehog_device_private.h"
#include <astarte_bson_serializer.h>
#include <esp_log.h>
#include <stdio.h>

static const char *TAG = "EDGEHOG_CELLULAR_CONNECTION";

const astarte_interface_t cellular_connection_status_interface
    = { .name = "io.edgehog.devicemanager.CellularConnectionStatus",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_DATASTREAM };

const astarte_interface_t cellular_connection_properties_interface
    = { .name = "io.edgehog.devicemanager.CellularConnectionProperties",
          .major_version = 0,
          .minor_version = 1,
          .ownership = OWNERSHIP_DEVICE,
          .type = TYPE_PROPERTIES };

static const char *edgehog_technology_to_string(edgehog_connection_technology technology);
static const char *edgehog_connection_status_to_string(edgehog_registration_status status);

void edgehog_connection_status_publish(edgehog_device_handle_t edgehog_device, char *modem_id,
    const char *carrier, edgehog_connection_technology technology,
    edgehog_registration_status registration_status, double rssi, int64_t cell_id,
    int local_area_code, int mobile_country_code, int mobile_network_code)
{
    astarte_bson_serializer_handle_t bs = astarte_bson_serializer_new();
    astarte_bson_serializer_append_string(bs, "carrier", carrier);
    astarte_bson_serializer_append_string(
        bs, "technology", edgehog_technology_to_string(technology));
    astarte_bson_serializer_append_string(
        bs, "registrationStatus", edgehog_connection_status_to_string(registration_status));
    astarte_bson_serializer_append_double(bs, "rssi", rssi);
    if (cell_id >= 0) {
        astarte_bson_serializer_append_int64(bs, "cellId", cell_id);
    }
    if (local_area_code >= 0) {
        astarte_bson_serializer_append_int32(bs, "localAreaCode", local_area_code);
    }
    if (mobile_country_code >= 0) {
        astarte_bson_serializer_append_int32(bs, "mobileCountryCode", mobile_country_code);
    }
    if (mobile_network_code >= 0) {
        astarte_bson_serializer_append_int32(bs, "mobileNetworkCode", mobile_network_code);
    }
    astarte_bson_serializer_append_end_of_document(bs);

    const void *doc = astarte_bson_serializer_get_document(bs, NULL);
    char *path = malloc(strlen(modem_id) + 2); // 2 = strlen("/") + 1
    if (!path) {
        ESP_LOGE(TAG, "Unable to allocate memory for path");
        return;
    }
    sprintf(path, "/%s", modem_id);
    astarte_err_t ret = astarte_device_stream_aggregate(
        edgehog_device->astarte_device, cellular_connection_status_interface.name, path, doc, 0);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish connection status");
    }
    free(path);
    astarte_bson_serializer_destroy(bs);
}

void edgehog_connection_properties_publish(edgehog_device_handle_t edgehog_device,
    const char *modem_id, const char *imei, const char *imsi, const char *apn)
{
    char path[32];
    if (snprintf(path, 32, "/%s/imei", modem_id) >= sizeof(path)) {
        ESP_LOGE(TAG, "Unable to publish imei property");
        return;
    }
    astarte_err_t ret = astarte_device_set_string_property(
        edgehog_device->astarte_device, cellular_connection_properties_interface.name, path, imei);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish imei property");
        return;
    }
    snprintf(path, 32, "/%s/imsi", modem_id);
    ret = astarte_device_set_string_property(
        edgehog_device->astarte_device, cellular_connection_properties_interface.name, path, imsi);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish imsi property");
        return;
    }
    snprintf(path, 32, "/%s/apn", modem_id);
    ret = astarte_device_set_string_property(
        edgehog_device->astarte_device, cellular_connection_properties_interface.name, path, apn);
    if (ret != ASTARTE_OK) {
        ESP_LOGE(TAG, "Unable to publish apn property");
    }
}

static const char *edgehog_technology_to_string(edgehog_connection_technology technology)
{
    switch (technology) {
        case GSM:
            return "GSM";
        case GSM_COMPACT:
            return "GSMCompact";
        case UTRAN:
            return "UTRAN";
        case GSM_WITH_EGPRS:
            return "GSMwEGPRS";
        case UTRAN_WITH_HSDPA:
            return "UTRANwHSDPA";
        case UTRAN_WITH_HSUPA:
            return "UTRANwHSUPA";
        case UTRAN_WITH_HSDPA_AND_HSUPA:
            return "UTRANwHSDPAandHSUPA";
        case E_UTRAN:
            return "EUTRAN";
        default:
            return "";
    }
}

static const char *edgehog_connection_status_to_string(edgehog_registration_status status)
{
    switch (status) {
        case NOT_REGISTERED:
            return "NotRegistered";
        case REGISTERED:
            return "Registered";
        case SEARCHING_OPERATOR:
            return "SearchingOperator";
        case REGISTRATION_DENIED:
            return "RegistrationDenied";
        case UNKNOWN:
            return "Unknown";
        case REGISTERED_ROAMING:
            return "RegisteredRoaming";
        default:
            return "";
    }
}

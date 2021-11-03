#include "astarte_handler.h"
#include <astarte_hwid.h>
#include <astarte_credentials.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <mbedtls/base64.h>
#include <uuid.h>

#define NVS_PARTITION "nvs"
#define EXAMPLE_UUID "37119eb1-84fc-4e4b-97de-0b18ab1a49f1"
#define MAC_LENGTH 6
#define ENCODED_HWID_LENGTH 64
#define MAC_STRING_LENGTH 13

static const char *TAG = "Astarte Handler";

static void astarte_data_events_handler(astarte_device_data_event_t *event)
{
    ESP_LOGI(TAG, "Got Astarte data event, interface_name: %s, path: %s, bson_type: %d",
        event->interface_name, event->path, event->bson_value_type);
}

static void astarte_connection_events_handler()
{
    ESP_LOGI(TAG, "on_connected");
}

static void astarte_disconnection_events_handler()
{
    ESP_LOGW(TAG, "on_disconnected");
}

astarte_device_handle_t astarte_handler_init()
{
    astarte_device_handle_t device = NULL;
    astarte_credentials_use_nvs_storage(NVS_PARTITION);
    astarte_credentials_init();

    char *encoded_hwid = astarte_handler_get_hardware_id_encoded();

    if (!encoded_hwid) {
        goto astarte_init_error;
    }

    ESP_LOGI(TAG, "Astarte Device ID -> %s", encoded_hwid);

    astarte_device_config_t cfg = { .data_event_callback = astarte_data_events_handler,
        .connection_event_callback = astarte_connection_events_handler,
        .disconnection_event_callback = astarte_disconnection_events_handler,
        .hwid = (char *) encoded_hwid };

    device = astarte_device_init(&cfg);
    free(encoded_hwid);

    if (!device) {
        ESP_LOGE(TAG, "Cannot to init astarte device");
        goto astarte_init_error;
    }

    esp_err_t err = add_interfaces(device);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to load Astarte interfaces");
        goto astarte_init_error;
    }

    return device;

astarte_init_error:
    if (device) {
        astarte_device_destroy(device);
    }
    return NULL;
}

esp_err_t add_interfaces(astarte_device_handle_t device)
{
    return ESP_OK;
}

char *astarte_handler_get_hardware_id_encoded()
{
    uint8_t mac[MAC_LENGTH];
    char mac_string[MAC_STRING_LENGTH];
    char *encoded_hwid = NULL;
    uuid_t namespace_uuid;
    uuid_t device_uuid;

    uuid_from_string(EXAMPLE_UUID, namespace_uuid);
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot get mac from wifi_STA (%s)", esp_err_to_name(err));
        return NULL;
    }
    snprintf(mac_string, MAC_STRING_LENGTH, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]);
    uuid_generate_v5(namespace_uuid, mac_string, strlen(mac_string), device_uuid);
    encoded_hwid = malloc(ENCODED_HWID_LENGTH);
    if (encoded_hwid == NULL) {
        ESP_LOGE(TAG, "Cannot allocate memory to init base64 buffer");
        return NULL;
    }
    astarte_hwid_encode(encoded_hwid, ENCODED_HWID_LENGTH, (const uint8_t *) device_uuid);
    return encoded_hwid;
}

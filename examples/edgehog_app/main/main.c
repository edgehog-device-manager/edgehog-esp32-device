#include "edgehog_device.h"
#include <astarte_credentials.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>

#define WIFI_CONNECTED_BIT BIT0
#define NVS_PARTITION "nvs"
#define TELEMETRY_DELAY_MS 1000 * 60 * 10

static const char *TAG = "CORE_WIFI";
static EventGroupHandle_t wifi_event_group;
static edgehog_device_handle_t edgehog_device;

static void event_handler(
    void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_ERROR_CHECK(
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(wifi_event_group);
}

static void astarte_connection_events_handler()
{
    ESP_LOGI(TAG, "on_connected");
}

static void astarte_disconnection_events_handler()
{
    ESP_LOGW(TAG, "on_disconnected");
}

static void astarte_data_events_handler(astarte_device_data_event_t *event)
{
    ESP_LOGI(TAG, "Got Astarte data event, interface_name: %s, path: %s, bson_type: %d",
        event->interface_name, event->path, event->bson_value_type);
    edgehog_device_astarte_event_handler(edgehog_device, event);
}

astarte_device_handle_t astarte_init()
{
    astarte_device_handle_t astarte_device = NULL;
    astarte_credentials_use_nvs_storage(NVS_PARTITION);
    astarte_credentials_init();

    astarte_device_config_t cfg = {
        .connection_event_callback = astarte_connection_events_handler,
        .disconnection_event_callback = astarte_disconnection_events_handler,
        .data_event_callback = astarte_data_events_handler,
    };

    astarte_device = astarte_device_init(&cfg);

    if (!astarte_device) {
        ESP_LOGE(TAG, "Failed to init astarte device");
        return NULL;
    }

    ESP_LOGI(TAG, "[APP] Encoded device ID: %s", astarte_device_get_encoded_id(astarte_device));

    return astarte_device;
}

static void telemetry_loop()
{
    vTaskDelay(pdMS_TO_TICKS(TELEMETRY_DELAY_MS));
    for (;;) {
        if (edgehog_device) {
            edgehog_device_publish_telemetry(edgehog_device, EH_TM_ALL);
        }
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_DELAY_MS));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init();
    ESP_ERROR_CHECK(nvs_flash_init());

    astarte_device_handle_t astarte_device = astarte_init();

    if (astarte_device && astarte_device_start(astarte_device) != ASTARTE_OK) {
        ESP_LOGE(TAG, "Failed to start astarte device");
        return;
    }

    edgehog_device_config_t edgehog_conf
        = { .astarte_device = astarte_device, .partition_label = "nvs" };
    edgehog_device = edgehog_device_new(&edgehog_conf);

    edgehog_device_set_appliance_serial_number(edgehog_device, "serial_number_1");
    edgehog_device_set_appliance_part_number(edgehog_device, "part_number_1");

    xTaskCreate(telemetry_loop, "telemetry", 4096, NULL, 5, NULL);
}

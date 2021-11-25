set(edgehog_srcs "src/edgehog_device.c"
        "src/edgehog_ota.c"
        "src/edgehog_storage_usage.c"
        "src/edgehog_battery_status.c")

idf_component_register(SRCS "${edgehog_srcs}"
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "private"
        REQUIRES astarte-device-sdk-esp32 nvs_flash app_update esp_https_ota)

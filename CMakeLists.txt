set(edgehog_srcs "src/edgehog_device.c"
        "src/edgehog_ota.c"
        "src/edgehog_storage_usage.c"
        "src/edgehog_battery_status.c"
        "src/edgehog_command.c"
        "src/edgehog_os_info.c"
        "src/edgehog_base_image.c"
        "src/edgehog_telemetry.c"
        "src/edgehog_runtime_info.c")

if (${CONFIG_INDICATOR_GPIO_ENABLE})
    set(edgehog_srcs ${edgehog_srcs} "src/edgehog_led.c")
endif ()

idf_component_register(SRCS "${edgehog_srcs}"
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "private"
        REQUIRES astarte-device-sdk-esp32 nvs_flash app_update esp_https_ota)

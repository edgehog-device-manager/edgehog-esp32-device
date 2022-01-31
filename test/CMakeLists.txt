set(srcs_test "test_edgehog_device.c"
        "test_edgehog_device_telemetry.c")

idf_component_register(SRCS "${srcs_test}"
        INCLUDE_DIRS "." "../include" "../private" "../src"
        REQUIRES unity astarte-device-sdk-esp32 nvs_flash)

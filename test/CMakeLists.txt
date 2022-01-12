set(srcs_test "test_edgehog_device.c")

idf_component_register(SRCS "${srcs_test}"
        INCLUDE_DIRS "."
        REQUIRES unity edgehog-esp32-device)

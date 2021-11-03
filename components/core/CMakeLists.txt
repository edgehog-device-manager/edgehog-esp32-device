set(core_srcs "src/core.c"
        "src/wifi/core_wifi.c")

idf_component_register(SRCS "${core_srcs}"
        INCLUDE_DIRS "include"
        REQUIRES nvs_flash)

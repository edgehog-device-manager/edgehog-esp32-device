set(devman_srcs "src/devman.c"
        "src/astarte_handler.c")

idf_component_register(SRCS "${devman_srcs}"
        INCLUDE_DIRS "include"
        REQUIRES astarte mbedtls)

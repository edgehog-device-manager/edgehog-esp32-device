cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "../../examples/edgehog_app/components"
        "../..")

set(test_components "edgehog-esp32-device")

set(TEST_COMPONENTS "${test_components}"
        STRING "List of components to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(edgehog-esp32-device-test)

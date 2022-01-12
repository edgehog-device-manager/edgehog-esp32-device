# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components"
        "${CMAKE_SOURCE_DIR}/../../..")

# Set the components to include the tests for.
# This can be overriden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#

set(test_components "edgehog-esp32-device")

set(TEST_COMPONENTS "${test_components}"
        STRING "List of components to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(edgehog-esp32-device-test)

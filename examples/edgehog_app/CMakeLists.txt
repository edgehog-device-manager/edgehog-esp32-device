# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_VER "0.1.0")

string(TIMESTAMP BUILD_DATE_TIME "%Y%m%d%H%M%S")
add_definitions(-DBUILD_DATE_TIME="${BUILD_DATE_TIME}")
project(edgehog-app)

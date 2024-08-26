#
# This file is part of Edgehog.
#
# Copyright 2021 SECO Mind Srl
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#

# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Get the absolute path of the current directory
get_filename_component(CURRENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
# Construct the absolute path to sdkconfig.priv
set(SDKCONFIG_PRIV_PATH "${CURRENT_DIR}/sdkconfig.priv")
# Construct the absolute path to sdkconfig.defaults
set(SDKCONFIG_DEFAULTS_PATH "${CURRENT_DIR}/sdkconfig.defaults")
# Check if sdkconfig.priv exists and add it to SDKCONFIG_DEFAULTS
if(EXISTS "${SDKCONFIG_PRIV_PATH}")
    if(EXISTS "${SDKCONFIG_DEFAULTS_PATH}")
        set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.priv")
    else()
        set(SDKCONFIG_DEFAULTS "sdkconfig.priv")
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_VER "0.1.0")

string(TIMESTAMP BUILD_DATE_TIME "%Y%m%d%H%M%S")
add_definitions(-DBUILD_DATE_TIME="${BUILD_DATE_TIME}")
project(edgehog-app)

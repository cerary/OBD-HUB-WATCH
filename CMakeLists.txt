# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
if(EXISTS "${CMAKE_SOURCE_DIR}/components")
	set(EXTRA_COMPONENT_DIRS components)
endif()
if(DEFINED ENV{STOPWATCH_DEPS_DIR})
    foreach(m5_component M5GFX M5PM1 M5IOE1)
        list(APPEND EXTRA_COMPONENT_DIRS "$ENV{STOPWATCH_DEPS_DIR}/${m5_component}")
    endforeach()
elseif(DEFINED SDKCONFIG AND SDKCONFIG MATCHES "stopwatch" AND EXISTS "${CMAKE_SOURCE_DIR}/../M5GFX")
    foreach(m5_component M5GFX M5PM1 M5IOE1)
        list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/../${m5_component}")
    endforeach()
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(DEFINED SDKCONFIG AND SDKCONFIG MATCHES "stopwatch")
    idf_build_set_property(MINIMAL_BUILD ON)
endif()
add_compile_options("-Wno-format")
project(obd_brz_gauge)

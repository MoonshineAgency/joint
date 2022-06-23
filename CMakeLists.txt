cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/esp-idf-lib/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_iot_node_plus)

cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/esp-idf-lib/components)
set(PROJECT_VER 0.1.0)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(joint)

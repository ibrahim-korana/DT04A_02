# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(PROJECT_VER "1.0.0.2")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(DT04A_02)
spiffs_create_partition_image(storage data FLASH_IN_PROJECT)
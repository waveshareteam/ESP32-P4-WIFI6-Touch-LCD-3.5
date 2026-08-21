# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_LIST_DIR}/../../../config/revision_profiles.cmake")
waveshare_configure_revision_profile("${CMAKE_CURRENT_LIST_DIR}")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(HowToCreateProject)

# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS components/)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(main)

# # Add the ESP32 WiFi library as a requirement
# idf_component_register(SRCS "wifi_manager.c"
#                        INCLUDE_DIRS "."
#                        REQUIRES esp_wifi)  # Specify the required components here
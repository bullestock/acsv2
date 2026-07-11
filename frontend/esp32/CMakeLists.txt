# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_definitions(-DESP_IDF -DESP32 -DDISABLE_ALL_LIBRARY_WARNINGS $ENV{ESP_EXTRA_DEFINITIONS})
add_compile_options(-Wno-missing-field-initializers)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

list(APPEND sdkconfig_defaults $ENV{IDF_PATH}/components/mbedtls/config/mbedtls_preset_default.conf)

project(frontend)

# Local Variables:
# compile-command: "idf.py build"
# End:

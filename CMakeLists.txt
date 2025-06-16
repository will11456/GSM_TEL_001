# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)



include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Get current Git tag or commit
execute_process(
  COMMAND git describe --tags --always
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Pass it to the compiler
add_compile_definitions(FW_GIT_VERSION="${GIT_VERSION}")


project(my_project)
project(GSM_TEL_001)

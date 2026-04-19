cmake_minimum_required(VERSION 3.22)

# This CMake file creates a host-based unit test executable using Catch2
# It is intended for development only and does not run on the embedded target.

# Option to enable tests at top level

add_library(test_common INTERFACE)
target_include_directories(test_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../App/driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../App/app
    ${CMAKE_CURRENT_SOURCE_DIR}/../App
    ${CMAKE_CURRENT_SOURCE_DIR}/../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/PY32F071_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/CMSIS/Device/PY32F071/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../App/external/CMSIS_5/Device/ARM/ARMCM0/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../App/external/CMSIS_5/CMSIS/Core/Include
    # catching framework removed; no extra include required
)

# Unit test executable
add_executable(unit_tests
    test_scheduler.cpp
    test_st7565.cpp
    test_update_flags.cpp
    test_runner.cpp
    extensive_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adc_stub.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dtmf_stub.c
)

target_link_libraries(unit_tests PRIVATE test_common pthread)

# unit_tests no longer uses Catch2; we provide our own simple harness

target_compile_options(unit_tests PRIVATE -Wall -Wextra -std=c++17)

# Register the test with CTest
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)

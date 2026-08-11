# Host-side unit tests for pure-logic firmware modules.
#
# These tests compile the actual firmware source files (frequencies.c,
# dcs.c, driver/crc.c) on the host using the system C compiler, so the
# tested code is identical to what runs on the radio.
#
# A test-local shim settings.h (in this directory) shadows the real
# App/settings.h, which pulls in heavy hardware dependencies that cannot
# be compiled on the host. The tests/ include path therefore precedes
# App/ in the include search order.
#
# This target is independent of the firmware build and is never linked
# into the firmware.

cmake_minimum_required(VERSION 3.19)
project(ApeX_UnitTests C)

enable_testing()

add_executable(unit_tests
    test_main.c
    test_frequencies.c
    test_dcs.c
    test_crc.c
    test_stubs.c
    # The actual firmware source files under test:
    ../App/frequencies.c
    ../App/dcs.c
    ../App/driver/crc.c
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include_shim  # centralized test shims shadow real headers
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../App
    ${CMAKE_CURRENT_SOURCE_DIR}/../App/driver
)

# Match the ApeX build preset (ENABLE_WIDE_RX=true) so the band table
# matches the production firmware.
target_compile_definitions(unit_tests PRIVATE
    ENABLE_WIDE_RX
)

add_test(NAME unit_tests COMMAND unit_tests)

# Static-analysis helper target.
# Runs cppcheck/clang-tidy via tools/static-analysis/run_static_analysis.sh if available.
# Intended for CI invocation: cmake --build build --target static-analysis
add_custom_target(static-analysis
    COMMAND ${CMAKE_SOURCE_DIR}/tools/static-analysis/run_static_analysis.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running static analysis (cppcheck/clang-tidy)..."
)

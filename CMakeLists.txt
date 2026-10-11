cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME f4hwn)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM)

option(DEV "Prepare the rolling Fusion development firmware" OFF)

find_package(Git QUIET)
set(BUILD_COMMIT "unknown")
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE BUILD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT BUILD_COMMIT)
        set(BUILD_COMMIT "unknown")
    endif()
endif()

# --- ApeX-only build identifier -------------------------------------------
# ApeX ships from this tree without a .git directory, so the stock probe above
# always yields "unknown". Only for the ApeX edition, let the build script
# inject the id explicitly (BUILD_COMMIT env var wins over local git
# detection) and fall back to a hex timestamp so the SysInf BUILD label always
# shows a real identifier. Fusion/FieldOps/Transfer/Labs/Max keep the vanilla
# behavior above, unchanged.
if("${EDITION_STRING}" STREQUAL "ApeX")
    if(DEFINED ENV{BUILD_COMMIT} AND NOT "$ENV{BUILD_COMMIT}" STREQUAL "" AND NOT "$ENV{BUILD_COMMIT}" STREQUAL "unknown")
        set(BUILD_COMMIT "$ENV{BUILD_COMMIT}")
    endif()

    if(BUILD_COMMIT STREQUAL "unknown")
        # Use Python to generate a hex timestamp that looks like a git commit hash
        execute_process(
            COMMAND python -c "import time, sys; sys.stdout.write(format(int(time.time()), '08x'))"
            OUTPUT_VARIABLE BUILD_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT BUILD_COMMIT)
            # If Python fails, fall back to a simple hex timestamp
            string(TIMESTAMP BUILD_COMMIT "%s")
            string(SUBSTRING "${BUILD_COMMIT}" 2 -1 BUILD_COMMIT_SUFFIX)
            set(BUILD_COMMIT "build${BUILD_COMMIT_SUFFIX}")
        endif()
    endif()

    file(WRITE "${CMAKE_BINARY_DIR}/BUILD_COMMIT.txt" "${BUILD_COMMIT}")
endif()

message(STATUS "Build commit: ${BUILD_COMMIT}")

if(ENABLE_FEAT_F4HWN)
    if(NOT AUTHOR_STRING_1)
        set(AUTHOR_STRING_1 "EGZUMER")
    endif()
    if(NOT AUTHOR_STRING_2)
        set(AUTHOR_STRING_2 "F4HWN")
    endif()
    if(NOT VERSION_STRING_1)
        set(VERSION_STRING_1 "v0.22")
    endif()
    if(NOT VERSION_STRING_2)
        set(VERSION_STRING_2 "v4.2")
    endif()
    if(NOT EDITION_STRING)
        set(EDITION_STRING "Custom")
    endif()
    if(NOT AUTHOR_STRING)
        set(AUTHOR_STRING "${AUTHOR_STRING_1}+${AUTHOR_STRING_2}")
    endif()
    if(NOT VERSION_STRING)
        set(VERSION_STRING ${VERSION_STRING_2})
    endif()
else()
    if(NOT AUTHOR_STRING)
        set(AUTHOR_STRING "EGZUMER")
    endif()
    if(NOT VERSION_STRING)
        set(VERSION_STRING "NOGIT")
    endif()
endif()

if(TARGET)
    set(EXE_NAME ${TARGET})
else()
    set(EXE_NAME "firmware")
endif()

if(DEV)
    if(NOT "${TARGET}" STREQUAL "f4hwn.fusion")
        message(FATAL_ERROR "DEV is only supported for the Fusion target")
    endif()
    set(DISPLAY_VERSION_STRING_2 "${VERSION_STRING_2} Dev")
else()
    set(DISPLAY_VERSION_STRING_2 "${VERSION_STRING_2}")
endif()

message("EXE_NAME = " ${EXE_NAME})
message("Development build = " ${DEV})

add_executable(${EXE_NAME})

add_subdirectory(Drivers)
add_subdirectory(Middlewares)
add_subdirectory(Core)
add_subdirectory(App)

target_link_libraries(${EXE_NAME} Core App)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PRINTF_FORMAT_CHECK_ARGS
    --source-root "${CMAKE_SOURCE_DIR}/App"
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND PRINTF_FORMAT_CHECK_ARGS --define DEBUG)
endif()
add_custom_target(check_printf_formats
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/cmake/check_printf_formats.py"
            ${PRINTF_FORMAT_CHECK_ARGS}
    DEPENDS "${CMAKE_SOURCE_DIR}/cmake/check_printf_formats.py"
    COMMENT "Validating minimal printf formats"
    VERBATIM
)
add_dependencies(${EXE_NAME} check_printf_formats)

target_compile_definitions(${EXE_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_link_libraries(${EXE_NAME} ${TOOLCHAIN_LINK_LIBRARIES})

if(ENABLE_LTO)
    target_compile_options(${EXE_NAME} PRIVATE -flto=auto)
endif()

# Generate map file
target_link_options(${EXE_NAME} PRIVATE "-Wl,-Map=${EXE_NAME}.map")

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${EXE_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${EXE_NAME}.map)

# Force linker to ignore RWX segment warning even if cache exists
target_link_options(${EXE_NAME} PRIVATE -Wl,--no-warn-rwx-segment)

# ENABLE_NO_HEAP: no malloc/sbrk is linked (libc is discarded in py32f071xb.ld),
# so the reserved heap is dead weight. Zero it to reclaim ~512 B of RAM. This is
# a global toolchain optimization, independent of multiboot. The linker script
# reads __min_heap_size when defined, else keeps its 0x200 default.
if(ENABLE_NO_HEAP)
    target_link_options(${EXE_NAME} PRIVATE "-Wl,--defsym=__min_heap_size=0")
endif()

# -----------------------------------
#  Post build processing
#

# The restore stub executes while application flash is unavailable. Reject an
# overlay build if the linked stub branches outside its RAM section or if its
# input section refers to any external code/data. This is deliberately a hard
# build gate: either condition could make a restore unrecoverable without DFU.
if(ENABLE_FEAT_F4HWN_MULTIBOOT_OVERLAY)
    add_custom_command(
        TARGET ${EXE_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DOBJDUMP=${CMAKE_OBJDUMP}"
                "-DELF=$<TARGET_FILE:${EXE_NAME}>"
                "-DMAP=${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME}.map"
                "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/check_mb_ramfunc.cmake"
        COMMENT "Checking multiboot RAM stub isolation"
        VERBATIM
    )
endif()

# Generate .bin file
add_custom_command(
    TARGET ${EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXE_NAME}.elf ${EXE_NAME}.bin
    COMMENT "Generating .bin file"
)

if(DEV)
    add_custom_command(
        TARGET ${EXE_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME}.bin"
                "${CMAKE_SOURCE_DIR}/archive/f4hwn.fusion.development.bin"
        COMMENT "Preparing archive/f4hwn.fusion.development.bin"
    )
endif()

# Generate .hex file
add_custom_command(
    TARGET ${EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXE_NAME}.elf ${EXE_NAME}.hex
    COMMENT "Generating .hex file"
)

# Pack
# TODO: Incorparate venv?
# if(ENABLE_FEAT_F4HWN)
#     add_custom_command(
#         TARGET ${EXE_NAME}
#         POST_BUILD
#         COMMAND python ${CMAKE_SOURCE_DIR}/fw-pack.py ${TARGET_ELF} ${AUTHOR_STRING_2} ${VERSION_STRING_2} ${EXE_NAME}.packed.bin
#         COMMENT "Generating packed binary"
#     )
# else()
#     add_custom_command(
#         TARGET ${EXE_NAME}
#         POST_BUILD
#         COMMAND python ${CMAKE_SOURCE_DIR}/fw-pack.py ${TARGET_ELF} ${AUTHOR_STRING} ${VERSION_STRING} ${EXE_NAME}.packed.bin
#         COMMENT "Generating packed binary"
#     )
# endif()


add_library(Core STATIC
    Src/main.c
    Src/py32f071_it.c
    Src/system_py32f071.c
    Src/syscalls.c
    startup_py32f071xx.s
)

target_include_directories(Core PUBLIC Inc)
target_link_libraries(Core PUBLIC PY32F071_Driver)
target_compile_definitions(Core PUBLIC 
    PY32F071x8 
    USE_FULL_LL_DRIVER
)
target_link_options(Core PUBLIC "-T" "${CMAKE_CURRENT_SOURCE_DIR}/py32f071xb.ld")

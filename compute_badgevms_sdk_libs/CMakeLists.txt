cmake_minimum_required(VERSION 3.16)

set(SDK_COMPILER ${CMAKE_C_COMPILER})
set(SDK_CXX_COMPILER ${CMAKE_CXX_COMPILER})

if(NOT SDK_COMPILER OR SDK_COMPILER STREQUAL "")
    find_program(SDK_COMPILER riscv32-esp-elf-gcc REQUIRED)
    find_program(SDK_CXX_COMPILER riscv32-esp-elf-g++ REQUIRED)
endif()

message(STATUS "Building SDK libraries with compiler: ${SDK_COMPILER}")

set(SDK_COMPILE_FLAGS
    -O2
    -flto
    -fPIC
    -fdata-sections
    -ffunction-sections
    -fno-builtin
    -fno-builtin-function
    -fno-common
    -fno-jump-tables
    -fno-tree-switch-conversion
    -fstrict-volatile-bitfields
    -fvisibility=hidden
    -g3
    -mabi=ilp32f
    -march=rv32imafc_zicsr_zifencei
    -nostartfiles
    -nostdlib
    -shared
)

set(SDK_STAGING_DIR ${CMAKE_BINARY_DIR}/sdk_staging)
set(SDK_LIB_DIR ${SDK_STAGING_DIR}/lib)
set(SDK_INCLUDE_DIR ${SDK_STAGING_DIR}/include)

file(MAKE_DIRECTORY ${SDK_STAGING_DIR})
file(MAKE_DIRECTORY ${SDK_LIB_DIR})
file(MAKE_DIRECTORY ${SDK_INCLUDE_DIR})

file(COPY ${CMAKE_SOURCE_DIR}/compute_badgevms_sdk_include/ DESTINATION ${SDK_INCLUDE_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/compute_badgevms/include/ DESTINATION ${SDK_INCLUDE_DIR})

function(build_sdk_library lib_name)
    set(LIB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${lib_name})

    add_subdirectory(${lib_name})

    target_compile_options(${lib_name} PRIVATE ${SDK_COMPILE_FLAGS})
    target_include_directories(${lib_name} PRIVATE ${SDK_INCLUDE_DIR})

    add_custom_command(
        OUTPUT ${SDK_LIB_DIR}/lib${lib_name}.a
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${lib_name}> ${SDK_LIB_DIR}/lib${lib_name}.a
        DEPENDS ${lib_name}
        COMMENT "Copying lib${lib_name}.a to SDK staging"
        VERBATIM
    )

    add_custom_target(${lib_name}_sdk ALL DEPENDS ${SDK_LIB_DIR}/lib${lib_name}.a)

    set_property(GLOBAL APPEND PROPERTY SDK_LIBRARIES ${SDK_LIB_DIR}/lib${lib_name}.a)
    set_property(GLOBAL APPEND PROPERTY SDK_LIBRARY_TARGETS ${lib_name}_sdk)
endfunction()

build_sdk_library(sdl3)

get_property(SDK_LIBRARY_TARGETS GLOBAL PROPERTY SDK_LIBRARY_TARGETS)
add_custom_target(build_sdk ALL DEPENDS ${SDK_LIBRARY_TARGETS})

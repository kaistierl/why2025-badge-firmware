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

set_property(GLOBAL PROPERTY SDK_STAGING_DIR ${CMAKE_BINARY_DIR}/sdk_staging)
get_property(SDK_STAGING_DIR GLOBAL PROPERTY SDK_STAGING_DIR)

add_custom_command(
    OUTPUT ${SDK_STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E rm -rf -- ${SDK_STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_STAGING_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_STAGING_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/sdk_include ${SDK_STAGING_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/badgevms/include ${SDK_STAGING_DIR}/include
    DEPENDS
     ${CMAKE_SOURCE_DIR}/sdk_include
     ${CMAKE_SOURCE_DIR}/badgevms
    VERBATIM
)
add_custom_target(init_sdk_staging ALL
    DEPENDS ${SDK_STAGING_DIR}
)
add_custom_target(final_sdk_staging ALL
    DEPENDS init_sdk_staging
)

function(build_sdk_library lib_name)
    set(LIB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${lib_name})

    add_subdirectory(${lib_name})

    target_compile_options(${lib_name} PRIVATE ${SDK_COMPILE_FLAGS})
    target_include_directories(${lib_name} PRIVATE ${SDK_STAGING_DIR}/include)
    add_dependencies(${lib_name} init_sdk_staging)

    add_custom_target(sdk_staging_add_lib_${lib_name} ALL
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${lib_name}> ${SDK_STAGING_DIR}/lib/lib${lib_name}.a
        DEPENDS
         ${lib_name}
         init_sdk_staging
        BYPRODUCTS
         ${SDK_STAGING_DIR}/lib/lib${lib_name}.a
        COMMENT "Copying lib${lib_name}.a to SDK staging"
        VERBATIM
    )
    add_dependencies(final_sdk_staging sdk_staging_add_lib_${lib_name})

    set_property(GLOBAL APPEND PROPERTY SDK_LIBS ${SDK_STAGING_DIR}/lib/lib${lib_name}.a)
    set_property(GLOBAL APPEND PROPERTY SDK_LIB_TARGETS sdk_staging_add_lib_${lib_name})
endfunction()

build_sdk_library(sdl3)

get_property(SDK_LIB_TARGETS GLOBAL PROPERTY SDK_LIB_TARGETS)
add_custom_target(sdk_lib_build_all ALL DEPENDS ${SDK_LIB_TARGETS})

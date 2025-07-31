cmake_minimum_required(VERSION 3.16)

set(ENV{IDF_TARGET} esp32p4)

include(ExternalProject)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build compute_badgevms/ as compute_badgevms.bin
set(EXTRA_COMPONENT_DIRS compute_badgevms)
set(COMPONENTS compute_badgevms)
project(compute_badgevms)

# Build compute_badgevms_sdk_libs/
add_subdirectory(compute_badgevms_sdk_libs)

# Build compute_badgevms_sdk_apps/
# Outputs are copied to the compute_storage fatfs
add_subdirectory(compute_badgevms_sdk_apps)

# Build compute_storage/ as storage.bin
add_subdirectory(compute_storage)

# Build compute_factory/ as compute_factory.bin
ExternalProject_Add(compute_factory
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compute_factory
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compute_factory
    CONFIGURE_COMMAND IDF_TARGET=esp32p4 idf.py reconfigure
    BUILD_COMMAND     IDF_TARGET=esp32p4 idf.py app
                      && cp build/factory.bin ${CMAKE_CURRENT_BINARY_DIR}/compute_factory.bin
    BUILD_ALWAYS true
    INSTALL_COMMAND ""
)

# Build and run compute_badgevms_host_tests/
ExternalProject_Add(compute_badgevms_host_tests
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compute_badgevms_host_tests
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/compute_badgevms_host_tests
    BUILD_ALWAYS true
    INSTALL_COMMAND ""
    TEST_AFTER_INSTALL true
)

# Build connectivity_esp_hosted/ as network_adapter.bin
ExternalProject_Add(compute_recovery_c6
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/connectivity_esp_hosted/slave
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/connectivity_esp_hosted/slave
    CONFIGURE_COMMAND IDF_TARGET=esp32c6 idf.py reconfigure
    BUILD_COMMAND     IDF_TARGET=esp32c6 idf.py merge-bin
    BUILD_ALWAYS true
    INSTALL_COMMAND ""
)

# Custom target for generating sdk_dist/
add_custom_target(sdk
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/sdk_staging ${CMAKE_SOURCE_DIR}/sdk_dist
    DEPENDS build_sdk ${CMAKE_SOURCE_DIR}/compute_badgevms_sdk_include/ ${CMAKE_SOURCE_DIR}/compute_badgevms/include
    COMMENT "Creating SDK distribution in sdk_dist/"
)

# Add our binaries to the flash target
# bootloader, partition-table and otadata are done automatically
esptool_py_flash_to_partition(flash "factory" "${CMAKE_CURRENT_BINARY_DIR}/compute_factory.bin")
esptool_py_flash_to_partition(flash "ota_0"   "${CMAKE_CURRENT_BINARY_DIR}/compute_badgevms.bin")
esptool_py_flash_to_partition(flash "storage" "${CMAKE_CURRENT_BINARY_DIR}/storage.bin")

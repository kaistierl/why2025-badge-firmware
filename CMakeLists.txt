cmake_minimum_required(VERSION 3.16)

set(ENV{IDF_TARGET} esp32p4)

cmake_policy(SET CMP0114 NEW)

include(ExternalProject)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build compute_badgevms/
#  outputs: compute_badgevms.bin
#  targets: gen_project_binary
set(EXTRA_COMPONENT_DIRS compute_badgevms)
set(COMPONENTS compute_badgevms)
project(compute_badgevms)

# Build and run compute_badgevms_host_tests/
#  targets: compute_badgevms_host_tests-test
ExternalProject_Add(compute_badgevms_host_tests
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/compute_badgevms_host_tests
    BINARY_DIR ${CMAKE_BINARY_DIR}/compute_badgevms_host_tests
    BUILD_ALWAYS true
    INSTALL_COMMAND ""
    TEST_AFTER_INSTALL true
)
ExternalProject_Add_StepTargets(compute_badgevms_host_tests test)
add_dependencies(gen_project_binary compute_badgevms_host_tests-test)

# Build compute_storage/
# outputs: storage.bin
# targets: init_storage_staging, final_storage_staging, fatfs_storage_bin
add_subdirectory(compute_storage)

# Build compute_badgevms_sdk_libs/
# outputs: SDK_LIBS
# targets: SDK_LIB_BUILD_TARGETS, sdk_lib_build_all
# targets: init_sdk_staging, final_sdk_staging
add_subdirectory(compute_badgevms_sdk_libs)

# Custom target for generating sdk_dist/
# TODO: might want to split this into a subdirectory, along with init/final_sdk_staging
get_property(SDK_STAGING_DIR GLOBAL PROPERTY SDK_STAGING_DIR)
add_custom_target(sdk
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SDK_STAGING_DIR} ${CMAKE_SOURCE_DIR}/sdk_dist
    DEPENDS final_sdk_staging
    COMMENT "Creating SDK distribution in sdk_dist/"
)

# Build compute_badgevms_sdk_apps/
#  output: SDK_APP_ELFS
#  target: SDK_APP_BUILD_TARGETS, sdk_app_build_all
add_subdirectory(compute_badgevms_sdk_apps)

# Build connectivity_esp_hosted/ as network_adapter.bin (target )
#  output: connectivity.bin
#  target: connectivity_esp_hosted-install
ExternalProject_Add(connectivity_esp_hosted
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/connectivity_esp_hosted/slave
    BINARY_DIR ${CMAKE_SOURCE_DIR}/connectivity_esp_hosted/slave
    CONFIGURE_COMMAND IDF_TARGET=esp32c6 idf.py reconfigure
    BUILD_COMMAND IDF_TARGET=esp32c6 idf.py merge-bin
    BUILD_ALWAYS true
    BUILD_BYPRODUCTS build/network_adapter.bin
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy build/network_adapter.bin ${CMAKE_BINARY_DIR}/connectivity.bin
)
ExternalProject_Add_StepTargets(connectivity_esp_hosted install)
get_property(STORAGE_STAGING_DIR GLOBAL PROPERTY STORAGE_STAGING_DIR)
add_custom_command(
    OUTPUT ${STORAGE_STAGING_DIR}/firmware/connectivity.bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STORAGE_STAGING_DIR}/firmware
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/connectivity.bin ${STORAGE_STAGING_DIR}/firmware/connectivity.bin
    DEPENDS
     connectivity_esp_hosted-install
     init_storage_staging
    COMMENT "Copying connectivity.bin to storage staging"
    VERBATIM
)
add_custom_target(storage_staging_add_connectivity ALL
    DEPENDS ${STORAGE_STAGING_DIR}/firmware/connectivity.bin
)
add_dependencies(final_storage_staging storage_staging_add_connectivity)

# Add our binaries to the flash target
# bootloader, partition-table and otadata are done automatically
# esptool_py_flash_to_partition(flash "ota_0"   "${CMAKE_BINARY_DIR}/compute_badgevms.bin")
esptool_py_flash_to_partition(flash "storage" "${CMAKE_BINARY_DIR}/storage.bin")

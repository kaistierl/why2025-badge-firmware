cmake_minimum_required(VERSION 3.16)

set(ENV{IDF_TARGET} esp32p4)

cmake_policy(SET CMP0114 NEW)

include(ExternalProject)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build badgevms/
#  outputs: badgevms.bin
#  targets: gen_project_binary
set(EXTRA_COMPONENT_DIRS badgevms)
set(COMPONENTS badgevms)
project(badgevms)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    # Build and run host_tests/
    #  targets: host_tests-test
    ExternalProject_Add(host_tests
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/host_tests
        BINARY_DIR ${CMAKE_BINARY_DIR}/host_tests
        BUILD_ALWAYS true
        INSTALL_COMMAND ""
        TEST_AFTER_INSTALL true
    )
    ExternalProject_Add_StepTargets(host_tests test)
    add_dependencies(gen_project_binary host_tests-test)
endif()

# Build flash_storage/
# outputs: storage.bin
# targets: init_storage_staging, final_storage_staging, fatfs_storage_bin
add_subdirectory(flash_storage)

# Build sdk_libs/
# outputs: SDK_LIBS
# targets: SDK_LIB_BUILD_TARGETS, sdk_lib_build_all
# targets: init_sdk_staging, final_sdk_staging
add_subdirectory(sdk_libs)

# Custom target for generating sdk_dist/
# TODO: might want to split this into a subdirectory, along with init/final_sdk_staging
get_property(SDK_STAGING_DIR GLOBAL PROPERTY SDK_STAGING_DIR)
add_custom_target(sdk
    COMMAND ${CMAKE_COMMAND} -E rm -rf -- ${CMAKE_SOURCE_DIR}/sdk_dist
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SDK_STAGING_DIR} ${CMAKE_SOURCE_DIR}/sdk_dist
    DEPENDS final_sdk_staging
    COMMENT "Creating SDK distribution in sdk_dist/"
)

# Build sdk_apps/
#  output: SDK_APP_ELFS
#  target: SDK_APP_BUILD_TARGETS, sdk_app_build_all
add_subdirectory(sdk_apps)

# Build connectivity_esp_hosted/ as network_adapter.bin
#  output: connectivity.bin
#  target: connectivity_esp_hosted-install
ExternalProject_Add(connectivity_esp_hosted
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/connectivity_esp_hosted/slave
    BINARY_DIR ${CMAKE_SOURCE_DIR}/connectivity_esp_hosted/slave
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env IDF_TARGET=esp32c6 idf.py reconfigure
    BUILD_COMMAND ${CMAKE_COMMAND} -E env IDF_TARGET=esp32c6 idf.py build
    BUILD_ALWAYS true
    BUILD_BYPRODUCTS
     build/bootloader/bootloader.bin
     build/partition_table/partition-table.bin
     build/network_adapter.bin
    INSTALL_COMMAND
     ${CMAKE_COMMAND} -E rm -rf -- ${CMAKE_BINARY_DIR}/c6firmware
     && ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/c6firmware
     && ${CMAKE_COMMAND} -E copy build/network_adapter.bin build/partition_table/partition-table.bin build/bootloader/bootloader.bin ${CMAKE_BINARY_DIR}/c6firmware
)
ExternalProject_Add_StepTargets(connectivity_esp_hosted install)
# Copy to storage_staging and generate md5 hashes
file(WRITE ${CMAKE_BINARY_DIR}/generate_md5.cmake "
function(generate_md5_file input_file)
    file(MD5 \${input_file} file_md5)
    file(WRITE \${input_file}.md5 \"\${file_md5}\")
endfunction()

generate_md5_file(\${INPUT_FILE})
")
get_property(STORAGE_STAGING_DIR GLOBAL PROPERTY STORAGE_STAGING_DIR)
add_custom_target(storage_staging_add_connectivity ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/c6firmware/network_adapter.bin ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/c6firmware/partition-table.bin ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/c6firmware/bootloader.bin ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6
    # Generate MD5 files using CMake script (cross-platform)
    COMMAND ${CMAKE_COMMAND} -DINPUT_FILE=${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/network_adapter.bin -P ${CMAKE_BINARY_DIR}/generate_md5.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT_FILE=${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/partition-table.bin -P ${CMAKE_BINARY_DIR}/generate_md5.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT_FILE=${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/bootloader.bin -P ${CMAKE_BINARY_DIR}/generate_md5.cmake
    DEPENDS
     connectivity_esp_hosted-install
     init_storage_staging
    BYPRODUCTS
     ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/network_adapter.bin
     ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/partition-table.bin
     ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/bootloader.bin
     ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/network_adapter.bin.md5
     ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/partition-table.bin.md5
     ${STORAGE_STAGING_DIR}/BADGEVMS/APPS/why2025_firmware_ota_c6/bootloader.bin.md5
    COMMENT "Copying slave c6 firmware to storage staging and generating MD5 checksums"
    VERBATIM
)
add_dependencies(final_storage_staging storage_staging_add_connectivity)

# Add our binaries to the flash target
# bootloader, partition-table, otadata and ota_0 are done automatically
esptool_py_flash_to_partition(flash "storage" "${CMAKE_BINARY_DIR}/storage.bin")

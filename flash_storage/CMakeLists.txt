cmake_minimum_required(VERSION 3.16)

set(FATFS_PARTITION_NAME storage)

set_property(GLOBAL PROPERTY STORAGE_STAGING_DIR ${CMAKE_BINARY_DIR}/storage_staging)
get_property(STORAGE_STAGING_DIR GLOBAL PROPERTY STORAGE_STAGING_DIR)

add_custom_target(init_storage_staging ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf -- ${STORAGE_STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STORAGE_STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/skel ${STORAGE_STAGING_DIR}
    DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/skel
    BYPRODUCTS
     ${STORAGE_STAGING_DIR}
    VERBATIM
)
add_custom_target(final_storage_staging ALL
    DEPENDS init_storage_staging
)

fatfs_create_spiflash_image(
    ${FATFS_PARTITION_NAME}
    ${STORAGE_STAGING_DIR}
    PRESERVE_TIME
)
add_dependencies(fatfs_${FATFS_PARTITION_NAME}_bin
    final_storage_staging
)

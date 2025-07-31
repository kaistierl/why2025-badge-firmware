cmake_minimum_required(VERSION 3.16)

set(FATFS_PARTITION_NAME storage)
set(FATFS_STAGING_DIR ${CMAKE_BINARY_DIR}/compute_storage_staging)
set(FATFS_STATIC_DIR ${CMAKE_SOURCE_DIR}/compute_storage/skel)
file(MAKE_DIRECTORY ${FATFS_STAGING_DIR})

function(prepare_compute_storage_image)
    if(EXISTS ${FATFS_STATIC_DIR})
        file(COPY ${FATFS_STATIC_DIR}/ DESTINATION ${FATFS_STAGING_DIR})
    endif()

    get_property(ALL_APP_ELFS GLOBAL PROPERTY APP_ELFS)

    set(FATFS_STAGED_FILES "")
    set(APP_ELF_TARGETS "")

    foreach(APP_ELF ${ALL_APP_ELFS})
        get_filename_component(ELF_NAME ${APP_ELF} NAME)

        set(STAGED_FILE ${FATFS_STAGING_DIR}/${ELF_NAME})

        add_custom_command(
            OUTPUT ${STAGED_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy ${APP_ELF} ${STAGED_FILE}
            COMMAND ${CMAKE_STRIP} ${STAGED_FILE}
            DEPENDS ${APP_ELF}
            COMMENT "Copying ${ELF_NAME} to fatfs staging"
        )

        list(APPEND FATFS_STAGED_FILES ${STAGED_FILE})

        get_filename_component(ELF_NAME_WE ${APP_ELF} NAME_WE)
        list(APPEND APP_ELF_TARGETS ${ELF_NAME_WE}_elf)
    endforeach()

    add_custom_target(prepare_compute_storage_staging ALL
        DEPENDS ${FATFS_STAGED_FILES}
    )

    # Ensure the staging target depends on all app ELF targets
    if(APP_ELF_TARGETS)
        add_dependencies(prepare_compute_storage_staging ${APP_ELF_TARGETS})
    endif()
endfunction()

prepare_compute_storage_image()

fatfs_create_spiflash_image(
    ${FATFS_PARTITION_NAME}
    ${FATFS_STAGING_DIR}
    PRESERVE_TIME
)
add_dependencies(fatfs_${FATFS_PARTITION_NAME}_bin prepare_compute_storage_staging)

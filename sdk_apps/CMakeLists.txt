cmake_minimum_required(VERSION 3.16)

get_property(SDK_LIBRARIES GLOBAL PROPERTY SDK_LIBRARIES)
get_property(SDK_STAGING_DIR GLOBAL PROPERTY SDK_STAGING_DIR)
set(SDK_LIB_DIR ${SDK_STAGING_DIR}/lib)
set(SDK_INCLUDE_DIR ${SDK_STAGING_DIR}/include)

set(APP_COMPILE_FLAGS
    -O2
    -fPIC
    -flto
    -fdata-sections
    -ffunction-sections
    -fno-builtin
    -fno-builtin-function
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

set(APP_LINK_FLAGS
    -Wl,--strip-debug
    -Wl,--gc-sections
    -e main
)

set(APP_ELF_DIR ${CMAKE_BINARY_DIR}/app_elfs)
file(MAKE_DIRECTORY ${APP_ELF_DIR})

function(build_app app_name)
    cmake_parse_arguments(APP "" "" "SOURCES;LIBRARIES" ${ARGN})

    if(NOT APP_SOURCES)
        message(FATAL_ERROR "build_app: SOURCES must be specified for ${app_name}")
    endif()

    set(APP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)

    # Process sources
    set(ABSOLUTE_SOURCES "")
    foreach(SOURCE ${APP_SOURCES})
        if(IS_ABSOLUTE ${SOURCE})
            list(APPEND ABSOLUTE_SOURCES ${SOURCE})
        else()
            list(APPEND ABSOLUTE_SOURCES ${APP_SOURCE_DIR}/${SOURCE})
        endif()
    endforeach()

    set(LIBRARY_FLAGS "")
    if(APP_LIBRARIES)
        list(APPEND LIBRARY_FLAGS -L${SDK_LIB_DIR})
        foreach(LIB ${APP_LIBRARIES})
            list(APPEND LIBRARY_FLAGS -l${LIB})
            # This prevents unused symbols from ending up in the final elf
            list(APPEND LIBRARY_FLAGS -Wl,--exclude-libs,lib${LIB}.a)
        endforeach()
    endif()

    set(DEFINE_FLAGS "")
    if(APP_COMPILE_DEFINITIONS)
        foreach(DEF ${APP_COMPILE_DEFINITIONS})
            list(APPEND DEFINE_FLAGS -D${DEF})
        endforeach()
    endif()

    add_custom_command(
        OUTPUT ${APP_ELF_DIR}/${app_name}.elf
        COMMAND ${CMAKE_C_COMPILER}
            ${APP_COMPILE_FLAGS}
            ${APP_LINK_FLAGS}
            -isystem${SDK_INCLUDE_DIR}
            -o ${APP_ELF_DIR}/${app_name}.elf
            ${ABSOLUTE_SOURCES}
            ${LIBRARY_FLAGS}
            ${DEFINE_FLAGS}
        DEPENDS
         ${ABSOLUTE_SOURCES}
         final_sdk_staging
        COMMENT "Building app ELF: ${app_name}"
        VERBATIM
    )
    add_custom_target(build_app_${app_name} ALL
        DEPENDS
         ${APP_ELF_DIR}/${app_name}.elf
    )

    get_property(STORAGE_STAGING_DIR GLOBAL PROPERTY STORAGE_STAGING_DIR)
    add_custom_target(storage_staging_add_app_${app_name} ALL
        COMMAND ${CMAKE_COMMAND} -E copy ${APP_ELF_DIR}/${app_name}.elf ${STORAGE_STAGING_DIR}/${app_name}.elf
        COMMAND ${CMAKE_STRIP} ${STORAGE_STAGING_DIR}/${app_name}.elf
        DEPENDS
         build_app_${app_name}
         init_storage_staging
         ${APP_ELF_DIR}/${app_name}.elf
        BYPRODUCTS
         ${STORAGE_STAGING_DIR}/${app_name}.elf
        COMMENT "Copying ${app_name}.elf to storage staging and stripping"
        VERBATIM
    )
    add_dependencies(final_storage_staging storage_staging_add_app_${app_name})

    set_property(GLOBAL APPEND PROPERTY SDK_APP_ELFS ${APP_ELF_DIR}/${app_name}.elf)
    set_property(GLOBAL APPEND PROPERTY SDK_APP_BUILD_TARGETS build_app_${app_name})
endfunction()


build_app(hello_flasher
        SOURCES
        hello_flasher/hello_flasher.c
)

build_app(hello
    SOURCES
     hello/main.c
)

build_app(bench_basic_a
    SOURCES
     bench_basic_a/bench_basic_a.c
)

build_app(bench_basic_b
    SOURCES
     bench_basic_b/bench_basic_b.c
)

build_app(framebuffer_test
    SOURCES
     framebuffer_test/framebuffer_test_a.c
)

build_app(hardware_test
    SOURCES
     hardware_test/thirdparty/microui.c
     hardware_test/test_badge.c
     hardware_test/test_drawing_helper.c
     hardware_test/test_keyboard.c
     hardware_test/run_tests.c
)

build_app(sdl_test
    SOURCES
     sdl_test/sdl_test.c
    LIBRARIES
     sdl3
)

build_app(wifi_test
    SOURCES
     wifi_test/wifi_test.c
)

build_app(socket_test
    SOURCES
     socket_test/socket_test.c
)


build_app(curl_test
    SOURCES
     curl_test/curl_test.c
)

build_app(bmi270_test
    SOURCES
    bmi270_test/bmi270_test.c
)

build_app(thread_test
    SOURCES
     thread_test/main.c
)

build_app(process_test
    SOURCES
     process_test/main.c
)

build_app(appdb_test
    SOURCES
    appdb_test/main.c
)

build_app(readdir_test
    SOURCES
    readdir_test/main.c
)

#build_app(style_test
#    SOURCES
#     style_test/main.c
#     style_test/thirdparty/microui.c
#     style_test/test_drawing_helper.c
#)

build_app(ota_wifi_update
    SOURCES
     ota_wifi_update/ota_wifi_update.c
     ota_wifi_update/thirdparty/microui.c
)

build_app(sponsor_app
    SOURCES
     sponsor_app/main.c
     sponsor_app/image.c
)

build_app(doom
    SOURCES
     doomgeneric/doomgeneric/am_map.c
     doomgeneric/doomgeneric/d_event.c
     doomgeneric/doomgeneric/d_items.c
     doomgeneric/doomgeneric/d_iwad.c
     doomgeneric/doomgeneric/d_loop.c
     doomgeneric/doomgeneric/d_main.c
     doomgeneric/doomgeneric/d_mode.c
     doomgeneric/doomgeneric/d_net.c
     doomgeneric/doomgeneric/doomdef.c
     doomgeneric/doomgeneric/doomgeneric.c
     doomgeneric/doomgeneric/doomgeneric_badgevms.c
     doomgeneric/doomgeneric/doomstat.c
     doomgeneric/doomgeneric/dstrings.c
     doomgeneric/doomgeneric/dummy.c
     doomgeneric/doomgeneric/f_finale.c
     doomgeneric/doomgeneric/f_wipe.c
     doomgeneric/doomgeneric/g_game.c
     doomgeneric/doomgeneric/gusconf.c
     doomgeneric/doomgeneric/hu_lib.c
     doomgeneric/doomgeneric/hu_stuff.c
     doomgeneric/doomgeneric/i_cdmus.c
     doomgeneric/doomgeneric/icon.c
     doomgeneric/doomgeneric/i_endoom.c
     doomgeneric/doomgeneric/i_input.c
     doomgeneric/doomgeneric/i_joystick.c
     doomgeneric/doomgeneric/info.c
     doomgeneric/doomgeneric/i_scale.c
     doomgeneric/doomgeneric/i_sound.c
     doomgeneric/doomgeneric/i_system.c
     doomgeneric/doomgeneric/i_timer.c
     doomgeneric/doomgeneric/i_video.c
     doomgeneric/doomgeneric/m_argv.c
     doomgeneric/doomgeneric/m_bbox.c
     doomgeneric/doomgeneric/m_cheat.c
     doomgeneric/doomgeneric/m_config.c
     doomgeneric/doomgeneric/m_controls.c
     doomgeneric/doomgeneric/memio.c
     doomgeneric/doomgeneric/m_fixed.c
     doomgeneric/doomgeneric/m_menu.c
     doomgeneric/doomgeneric/m_misc.c
     doomgeneric/doomgeneric/m_random.c
     doomgeneric/doomgeneric/mus2mid.c
     doomgeneric/doomgeneric/p_ceilng.c
     doomgeneric/doomgeneric/p_doors.c
     doomgeneric/doomgeneric/p_enemy.c
     doomgeneric/doomgeneric/p_floor.c
     doomgeneric/doomgeneric/p_inter.c
     doomgeneric/doomgeneric/p_lights.c
     doomgeneric/doomgeneric/p_map.c
     doomgeneric/doomgeneric/p_maputl.c
     doomgeneric/doomgeneric/p_mobj.c
     doomgeneric/doomgeneric/p_plats.c
     doomgeneric/doomgeneric/p_pspr.c
     doomgeneric/doomgeneric/p_saveg.c
     doomgeneric/doomgeneric/p_setup.c
     doomgeneric/doomgeneric/p_sight.c
     doomgeneric/doomgeneric/p_spec.c
     doomgeneric/doomgeneric/p_switch.c
     doomgeneric/doomgeneric/p_telept.c
     doomgeneric/doomgeneric/p_tick.c
     doomgeneric/doomgeneric/p_user.c
     doomgeneric/doomgeneric/r_bsp.c
     doomgeneric/doomgeneric/r_data.c
     doomgeneric/doomgeneric/r_draw.c
     doomgeneric/doomgeneric/r_main.c
     doomgeneric/doomgeneric/r_plane.c
     doomgeneric/doomgeneric/r_segs.c
     doomgeneric/doomgeneric/r_sky.c
     doomgeneric/doomgeneric/r_things.c
     doomgeneric/doomgeneric/sha1.c
     doomgeneric/doomgeneric/sounds.c
     doomgeneric/doomgeneric/s_sound.c
     doomgeneric/doomgeneric/statdump.c
     doomgeneric/doomgeneric/st_lib.c
     doomgeneric/doomgeneric/st_stuff.c
     doomgeneric/doomgeneric/tables.c
     doomgeneric/doomgeneric/v_video.c
     doomgeneric/doomgeneric/w_checksum.c
     doomgeneric/doomgeneric/w_file.c
     doomgeneric/doomgeneric/w_file_stdc.c
     doomgeneric/doomgeneric/wi_stuff.c
     doomgeneric/doomgeneric/w_main.c
     doomgeneric/doomgeneric/w_wad.c
     doomgeneric/doomgeneric/z_zone.c
)

get_property(SDK_APP_ELFS GLOBAL PROPERTY SDK_APP_ELFS)
add_custom_target(sdk_app_build_all ALL DEPENDS ${SDK_APP_ELFS})

cmake_minimum_required(VERSION 3.16)

get_property(SDK_LIBRARIES GLOBAL PROPERTY SDK_LIBRARIES)
set(SDK_LIB_DIR ${CMAKE_BINARY_DIR}/sdk_staging/lib)
set(SDK_INCLUDE_DIR ${CMAKE_BINARY_DIR}/sdk_staging/include)

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
    set(LIBRARY_DEPENDENCIES "")
    if(APP_LIBRARIES)
        list(APPEND LIBRARY_FLAGS -L${SDK_LIB_DIR})
        foreach(LIB ${APP_LIBRARIES})
            list(APPEND LIBRARY_FLAGS -l${LIB})
            # This prevents unused symbols from ending up in the final elf
            list(APPEND LIBRARY_FLAGS -Wl,--exclude-libs,lib${LIB}.a)
            list(APPEND LIBRARY_DEPENDENCIES ${SDK_LIB_DIR}/lib${LIB}.a)
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
        DEPENDS ${ABSOLUTE_SOURCES} ${LIBRARY_DEPENDENCIES} ${SDK_INCLUDE_DIR}
        COMMENT "Building app ELF: ${app_name}"
        VERBATIM
    )

    add_custom_target(${app_name}_elf ALL DEPENDS ${APP_ELF_DIR}/${app_name}.elf)

    if(APP_LIBRARIES)
        add_dependencies(${app_name}_elf build_sdk)
    endif()

    set_property(GLOBAL APPEND PROPERTY APP_ELFS ${APP_ELF_DIR}/${app_name}.elf)
endfunction()

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

build_app(curl_test
    SOURCES
    curl_test/curl_test.c
)

build_app(bmi270_test
    SOURCES
    bmi270_test/bmi270_test.c
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

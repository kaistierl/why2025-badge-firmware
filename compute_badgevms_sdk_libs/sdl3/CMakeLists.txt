set(SDL3_SOURCES
    SDL3/src/SDL.c
    SDL3/src/SDL_assert.c
    SDL3/src/SDL_error.c
    SDL3/src/SDL_guid.c
    SDL3/src/SDL_hashtable.c
    SDL3/src/SDL_hints.c
    SDL3/src/SDL_list.c
    SDL3/src/SDL_log.c
    SDL3/src/SDL_properties.c
    SDL3/src/SDL_utils.c
    SDL3/src/atomic/SDL_spinlock.c
    SDL3/src/atomic/SDL_atomic.c
    # SDL3/src/audio/SDL_audio.c
    # SDL3/src/audio/SDL_audiocvt.c
    # SDL3/src/audio/SDL_audiodev.c
    # SDL3/src/audio/SDL_audioqueue.c
    # SDL3/src/audio/SDL_audioresample.c
    # SDL3/src/audio/SDL_audiotypecvt.c
    # SDL3/src/audio/SDL_mixer.c
    # SDL3/src/audio/SDL_wave.c
    # SDL3/src/audio/disk/SDL_diskaudio.c
    # SDL3/src/audio/dummy/SDL_dummyaudio.c
    # SDL3/src/camera/SDL_camera.c
    # SDL3/src/camera/dummy/SDL_camera_dummy.c
    # SDL3/src/core/SDL_core_unsupported.c
    SDL3/src/cpuinfo/SDL_cpuinfo.c
    # SDL3/src/dialog/SDL_dialog.c
    # SDL3/src/dialog/SDL_dialog_utils.c
    # SDL3/src/dialog/dummy/SDL_dummydialog.c
    SDL3/src/events/SDL_categories.c
    # SDL3/src/events/SDL_clipboardevents.c
    SDL3/src/events/SDL_displayevents.c
    SDL3/src/events/SDL_dropevents.c
    SDL3/src/events/SDL_events.c
    SDL3/src/events/SDL_eventwatch.c
    SDL3/src/events/SDL_keyboard.c
    SDL3/src/events/SDL_keymap.c
    SDL3/src/events/SDL_keysym_to_keycode.c
    SDL3/src/events/SDL_keysym_to_scancode.c
    SDL3/src/events/SDL_mouse.c
    SDL3/src/events/SDL_quit.c
    SDL3/src/events/SDL_scancode_tables.c
    SDL3/src/events/SDL_windowevents.c
    SDL3/src/events/SDL_pen.c
    SDL3/src/events/SDL_touch.c
    SDL3/src/events/imKStoUCS.c
    SDL3/src/filesystem/SDL_filesystem.c
    # SDL3/src/filesystem/dummy/SDL_sysfilesystem.c
    # SDL3/src/filesystem/dummy/SDL_sysfsops.c
    # SDL3/src/gpu/SDL_gpu.c
    # SDL3/src/hidapi/SDL_hidapi.c
    SDL3/src/io/SDL_asyncio.c
    SDL3/src/io/SDL_iostream.c
    # SDL3/src/joystick/SDL_gamepad.c
    SDL3/src/joystick/SDL_joystick.c
    # SDL3/src/joystick/SDL_steam_virtual_gamepad.c
    # SDL3/src/joystick/controller_type.c
    # SDL3/src/joystick/dummy/SDL_sysjoystick.c
    # SDL3/src/joystick/virtual/SDL_virtualjoystick.c
    SDL3/src/libm/e_atan2.c
    SDL3/src/libm/e_exp.c
    SDL3/src/libm/e_fmod.c
    SDL3/src/libm/e_log.c
    SDL3/src/libm/e_log10.c
    SDL3/src/libm/e_pow.c
    SDL3/src/libm/e_rem_pio2.c
    SDL3/src/libm/e_sqrt.c
    SDL3/src/libm/k_rem_pio2.c
    SDL3/src/libm/k_sin.c
    SDL3/src/libm/k_tan.c
    SDL3/src/libm/s_atan.c
    SDL3/src/libm/s_copysign.c
    SDL3/src/libm/s_fabs.c
    SDL3/src/libm/s_floor.c
    SDL3/src/libm/s_isinf.c
    SDL3/src/libm/s_isinff.c
    SDL3/src/libm/s_isnan.c
    SDL3/src/libm/s_isnanf.c
    SDL3/src/libm/s_modf.c
    SDL3/src/libm/s_scalbn.c
    SDL3/src/libm/s_sin.c
    SDL3/src/libm/s_tan.c
    SDL3/src/loadso/dummy/SDL_sysloadso.c
    SDL3/src/locale/SDL_locale.c
    SDL3/src/locale/dummy/SDL_syslocale.c
    SDL3/src/main/SDL_runapp.c
    SDL3/src/misc/SDL_url.c
    SDL3/src/misc/dummy/SDL_sysurl.c
    SDL3/src/power/SDL_power.c
    # SDL3/src/render/SDL_d3dmath.c
    SDL3/src/render/SDL_render.c
    # SDL3/src/render/SDL_render_unsupported.c
    SDL3/src/render/SDL_yuv_sw.c
    SDL3/src/render/software/SDL_blendfillrect.c
    SDL3/src/render/software/SDL_blendline.c
    SDL3/src/render/software/SDL_blendpoint.c
    SDL3/src/render/software/SDL_drawline.c
    SDL3/src/render/software/SDL_drawpoint.c
    SDL3/src/render/software/SDL_render_sw.c
    SDL3/src/render/software/SDL_rotate.c
    SDL3/src/render/software/SDL_triangle.c
    SDL3/src/thread/SDL_thread.c
    SDL3/src/time/SDL_time.c
    SDL3/src/timer/SDL_timer.c
    SDL3/src/tray/SDL_tray_utils.c
    SDL3/src/tray/dummy/SDL_tray.c
    SDL3/src/video/SDL_RLEaccel.c
    SDL3/src/video/SDL_blit.c
    SDL3/src/video/SDL_blit_0.c
    SDL3/src/video/SDL_blit_1.c
    SDL3/src/video/SDL_blit_A.c
    SDL3/src/video/SDL_blit_N.c
    SDL3/src/video/SDL_blit_auto.c
    SDL3/src/video/SDL_blit_copy.c
    SDL3/src/video/SDL_blit_slow.c
    SDL3/src/video/SDL_bmp.c
    SDL3/src/video/SDL_clipboard.c
    SDL3/src/video/SDL_fillrect.c
    SDL3/src/video/SDL_pixels.c
    SDL3/src/video/SDL_rect.c
    SDL3/src/video/SDL_stb.c
    SDL3/src/video/SDL_stretch.c
    SDL3/src/video/SDL_surface.c
    SDL3/src/video/SDL_video.c
    SDL3/src/video/SDL_yuv.c
    SDL3/src/video/yuv2rgb/yuv_rgb_lsx.c
    SDL3/src/video/yuv2rgb/yuv_rgb_sse.c
    SDL3/src/video/yuv2rgb/yuv_rgb_std.c

    SDL3/src/stdlib/SDL_crc16.c
    SDL3/src/stdlib/SDL_crc32.c
    SDL3/src/stdlib/SDL_getenv.c
    SDL3/src/stdlib/SDL_iconv.c
    SDL3/src/stdlib/SDL_malloc.c
    SDL3/src/stdlib/SDL_memcpy.c
    SDL3/src/stdlib/SDL_memmove.c
    SDL3/src/stdlib/SDL_memset.c
    SDL3/src/stdlib/SDL_mslibc.c
    SDL3/src/stdlib/SDL_murmur3.c
    SDL3/src/stdlib/SDL_qsort.c
    SDL3/src/stdlib/SDL_random.c
    SDL3/src/stdlib/SDL_stdlib.c
    SDL3/src/stdlib/SDL_string.c
    SDL3/src/stdlib/SDL_strtokr.c

    SDL3/src/thread/generic/SDL_syscond.c
    SDL3/src/thread/generic/SDL_sysmutex.c
    SDL3/src/thread/generic/SDL_sysrwlock.c
    SDL3/src/thread/generic/SDL_syssem.c
    SDL3/src/thread/generic/SDL_systhread.c
    SDL3/src/thread/generic/SDL_systls.c

    SDL3/src/main/generic/SDL_sysmain_callbacks.c
    SDL3/src/main/SDL_main_callbacks.c

    SDL3/src/timer/unix/SDL_systimer.c

    SDL3/src/process/dummy/SDL_dummyprocess.c
    SDL3/src/io/generic/SDL_asyncio_generic.c
    SDL3/src/sensor/dummy/SDL_dummysensor.c
    SDL3/src/sensor/SDL_sensor.c

    src/video/SDL_badgevmsframebuffer.c
    src/video/SDL_badgevmsevents.c
    src/video/SDL_badgevmsvideo.c
)

add_library(sdl3 STATIC ${SDL3_SOURCES})

target_compile_definitions(sdl3 PRIVATE
)

target_include_directories(sdl3 PRIVATE
  include
  SDL3/src
  SDL3/src/video
  SDL3/include/SDL3
)

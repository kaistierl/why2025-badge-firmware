# cmake configuration for local build of sshterm
# To build for the badge hardware, use the ESP-IDF build system as part of the main firmware instead.

cmake_minimum_required(VERSION 3.16)
project(sshterm)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable compiler warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Add size optimization flags
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,--gc-sections -Wl,--strip-all -s")

# Find SDL3 using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

# wolfSSL library (cryptonly build for wolfSSH)
file(GLOB WOLFSSL_SOURCES "thirdparty/wolfssl-5.8.2/wolfcrypt/src/*.c")
add_library(wolfssl STATIC ${WOLFSSL_SOURCES})

target_include_directories(wolfssl PUBLIC
    thirdparty/wolfssl-5.8.2
    components/ssh_client # for user_settings.h
)

target_compile_definitions(wolfssl PUBLIC
    WOLFSSL_USER_SETTINGS
)

# wolfSSH library
file(GLOB WOLFSSH_SOURCES "thirdparty/wolfssh-1.4.20/src/*.c")
add_library(wolfssh STATIC ${WOLFSSH_SOURCES})

target_include_directories(wolfssh PUBLIC
    thirdparty/wolfssh-1.4.20
)

target_link_libraries(wolfssh PUBLIC wolfssl)

# Define the executable
add_executable(sshterm
    main.c
    # Badge-specific crypto implementations
    components/ssh_client/badge_crypto_port.c
     # Custom I/O implementation for wolfSSL/wolfSSH
    components/ssh_client/custom_io.c
    components/input_system/input_system.c
    components/ssh_manager/ssh_manager.c
    components/ssh_manager/ssh_thread.c
    components/ssh_manager/ssh_ui_controller.c
    components/app_controller/app_controller.c
    components/ui_manager/ui_manager.c
    components/renderer/renderer.c
    components/term/term.c
    components/keyboard/keyboard.c
    components/ssh_client/ssh_client.c
    components/test_mode/test_mode.c
    thirdparty/libvterm-0.3.3/src/vterm.c
    thirdparty/libvterm-0.3.3/src/screen.c
    thirdparty/libvterm-0.3.3/src/state.c
    thirdparty/libvterm-0.3.3/src/pen.c
    thirdparty/libvterm-0.3.3/src/encoding.c
    thirdparty/libvterm-0.3.3/src/parser.c
    thirdparty/libvterm-0.3.3/src/keyboard.c
    thirdparty/libvterm-0.3.3/src/unicode.c
)

# Include directories
target_include_directories(sshterm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../badgevms/include
    ${CMAKE_CURRENT_SOURCE_DIR}/components
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libvterm-0.3.3/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/wolfssh-1.4.20
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/wolfssl-5.8.2
    ${SDL3_INCLUDE_DIRS}
)

# Link libraries and apply compile flags
target_link_libraries(sshterm PRIVATE ${SDL3_LIBRARIES} wolfssh wolfssl)
target_compile_options(sshterm PRIVATE ${SDL3_CFLAGS_OTHER})

# Add SDL3 link directories if needed
if(SDL3_LIBRARY_DIRS)
    target_link_directories(sshterm PRIVATE ${SDL3_LIBRARY_DIRS})
endif()

# Add fullscreen compile flag
option(BADGEVMS_FULLSCREEN "Enable fullscreen mode" OFF)
if(BADGEVMS_FULLSCREEN)
    target_compile_definitions(sshterm PRIVATE BADGEVMS_FULLSCREEN=1)
endif()

# Set output directory
set_target_properties(sshterm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

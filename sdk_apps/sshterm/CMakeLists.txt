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

# Create mbedTLS library
file(GLOB MBEDTLS_SOURCES 
    "thirdparty/mbedtls-3.6.4/library/*.c"
)
# Exclude some files that cause conflicts or are not needed
list(REMOVE_ITEM MBEDTLS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/net_sockets.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/timing.c"
    # Exclude SSL/TLS modules we disabled
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_cache.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_cookie.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_ticket.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_cli.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_srv.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls12_client.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls12_server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls13_client.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls13_server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls13_keys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_tls13_generic.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_debug_helpers_generated.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ssl_msg.c"
    # Exclude X.509 modules we disabled
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509_crt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509_crl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509_csr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509_create.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509write.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509write_crt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/x509write_csr.c"
    # Exclude unused crypto algorithms
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/aria.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/blowfish.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/camellia.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/des.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/ripemd160.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/sha3.c"
    # Exclude PKCS modules we don't need
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/pkcs5.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/pkcs7.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/library/pkcs12.c"
)
add_library(mbedtls STATIC ${MBEDTLS_SOURCES})
target_include_directories(mbedtls PUBLIC
    thirdparty/mbedtls-3.6.4/include
)
target_include_directories(mbedtls PRIVATE
    components/ssh_client
)
target_compile_definitions(mbedtls PRIVATE
    MBEDTLS_CONFIG_FILE="mbedtls_config_optimized.h"
)

# Create libssh2 library
file(GLOB LIBSSH2_SOURCES 
    "thirdparty/libssh2-1.11.1/src/*.c"
)
# Remove platform-specific files we don't need
list(REMOVE_ITEM LIBSSH2_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/openssl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/libgcrypt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/wincng.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/os400qc3.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/agent_win.c"
    # Remove SFTP and SCP modules we disabled
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/sftp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/src/scp.c"
)
add_library(libssh2 STATIC ${LIBSSH2_SOURCES})
target_include_directories(libssh2 PUBLIC
    thirdparty/libssh2-1.11.1/include
)
target_include_directories(libssh2 PRIVATE
    thirdparty/libssh2-1.11.1/src
    components/ssh_client
)
target_compile_definitions(libssh2 PRIVATE
    LIBSSH2_MBEDTLS
    HAVE_CONFIG_H
)
target_link_libraries(libssh2 PRIVATE mbedtls)

# Define the executable
add_executable(sshterm
    main.c
    components/input_system/input_system.c
    components/ssh_manager/ssh_manager.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2-1.11.1/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mbedtls-3.6.4/include
    ${SDL3_INCLUDE_DIRS}
)

# Add compile definitions
target_compile_definitions(sshterm PRIVATE
    HAVE_CONFIG_H
)

# Link SDL3 libraries and apply compile flags
target_link_libraries(sshterm PRIVATE ${SDL3_LIBRARIES} libssh2 mbedtls)
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

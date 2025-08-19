# cmake configuration for local build of sshterm
# To build for the badge hardware, use the ESP-IDF build system as part of the main firmware instead.

cmake_minimum_required(VERSION 3.16)
project(sshterm)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable compiler warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Find SDL3 using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

# Define the executable
add_executable(sshterm
    main.c
    components/renderer/renderer.c
    components/term/term.c
    components/keyboard/keyboard.c
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
    ${SDL3_INCLUDE_DIRS}
)

# Add compile definitions
target_compile_definitions(sshterm PRIVATE
    HAVE_CONFIG_H
)

# Link SDL3 libraries and apply compile flags
target_link_libraries(sshterm PRIVATE ${SDL3_LIBRARIES})
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

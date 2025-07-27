idf_component_register(
    SRCS
	 ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
	 "buddy_alloc.c"
	 "compositor/compositor.c"
	 "compositor/pixel_functions.c"
	 "compositor/window_decorations.c"
	 "device.c"
	 "drivers/fatfs.c"
	 "drivers/st7703.c"
	 "drivers/tca8418.c"
	 "drivers/tty.c"
	 "logical_names.c"
	 "memory.c"
	 "memory_heap_caps.c"
	 "pathfuncs.c"
	 "task.c"
	 "thirdparty/dlmalloc.c"
	 "user_event.c"
	 "wrapped_funcs.c"
    	 "why2025_firmware.c"
    INCLUDE_DIRS
	 ${CMAKE_CURRENT_SOURCE_DIR}/
	 ${CMAKE_CURRENT_SOURCE_DIR}/../include/badgevms
	 # We really don't want to include ../include in BadgeVMS itself
	 ${CMAKE_CURRENT_SOURCE_DIR}/../components/sdl3/SDL3/include
	 ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    EMBED_FILES
    	"${CMAKE_BINARY_DIR}/test_elfs/framebuffer_test_a.elf"
	"${CMAKE_BINARY_DIR}/test_elfs/sdl_test.elf"
	"${CMAKE_BINARY_DIR}/test_elfs/lvgl_test.elf"
#    	"${CMAKE_BINARY_DIR}/test_elfs/test_basic_a.elf"
#	"${CMAKE_BINARY_DIR}/test_elfs/test_basic_b.elf"
#	"${CMAKE_BINARY_DIR}/test_elfs/test_basic_c.elf"
#	"${CMAKE_BINARY_DIR}/test_elfs/test_shell.elf"
	"${CMAKE_BINARY_DIR}/test_elfs/bench_basic_a.elf"
	"${CMAKE_BINARY_DIR}/test_elfs/bench_basic_b.elf"
    PRIV_REQUIRES
	"esp_driver_ppa"
	"esp_lcd_st7703"
	"esp_psram"
	"esp_tca8418"
	"fatfs"
	"hal"
	"vfs"
        "esp_mm"
)

set(TEST_COMPILER ${CMAKE_C_COMPILER})
if(NOT TEST_COMPILER OR TEST_COMPILER STREQUAL "")
    find_program(TEST_COMPILER riscv32-esp-elf-gcc REQUIRED)
endif()

message(STATUS "Building test ELFs with compiler: ${TEST_COMPILER}")

set(TEST_ELF_DIR ${CMAKE_BINARY_DIR}/test_elfs)
file(MAKE_DIRECTORY ${TEST_ELF_DIR})

function(build_test_elf name source)
    add_custom_command(
        OUTPUT ${TEST_ELF_DIR}/${name}.elf
        COMMAND ${TEST_COMPILER}
	    -march=rv32imafc_zicsr_zifencei
            -mabi=ilp32f
	    -ffunction-sections
            -fdata-sections
	    -nostartfiles
	    -fno-builtin
	    -fno-builtin-function
            -Og
	    -g3
            -fstrict-volatile-bitfields
            -fno-jump-tables
            -fno-tree-switch-conversion
            -nostdlib
            -fPIC
            -shared
            -fvisibility=hidden
	    -Wl,--gc-sections
	    -e main
	    -isystem${CMAKE_CURRENT_SOURCE_DIR}/../include
	    -I${CMAKE_CURRENT_SOURCE_DIR}/../include/badgevms
            -o ${TEST_ELF_DIR}/${name}.elf
            ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        COMMENT "Building test ELF: ${name}"
        VERBATIM
    )
    
    add_custom_target(${name}_elf ALL DEPENDS ${TEST_ELF_DIR}/${name}.elf)
endfunction()

function(build_test_elf_lvgl name source)
    add_custom_command(
        OUTPUT ${TEST_ELF_DIR}/${name}.elf
        COMMAND ${TEST_COMPILER}
	    -march=rv32imafc_zicsr_zifencei
            -mabi=ilp32f
	    -ffunction-sections
            -fdata-sections
	    -nostartfiles
	    -fno-builtin
	    -fno-builtin-function
            -Og
	    -g3
            -fstrict-volatile-bitfields
            -fno-jump-tables
            -fno-tree-switch-conversion
            -nostdlib
            -fPIC
            -shared
            -fvisibility=hidden
	    -Wl,--gc-sections
	    -Wl,--strip-debug
	    -Wl,--exclude-libs,liblvgl.a
	    -e main
	    -isystem${CMAKE_CURRENT_SOURCE_DIR}/../include
	    -I${CMAKE_CURRENT_SOURCE_DIR}/../include/badgevms
            -o ${TEST_ELF_DIR}/${name}.elf
            ${CMAKE_CURRENT_SOURCE_DIR}/${source}
	    ${CMAKE_CURRENT_SOURCE_DIR}/test_elf/liblvgl.a
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        COMMENT "Building test ELF: ${name}"
        VERBATIM
    )
    
    add_custom_target(${name}_elf ALL DEPENDS ${TEST_ELF_DIR}/${name}.elf)
endfunction()

#build_test_elf(test_basic_a test_elf/test_basic_a.c)
#build_test_elf(test_basic_b test_elf/test_basic_b.c)
#build_test_elf(test_basic_c test_elf/test_basic_c.c)
#build_test_elf(test_shell test_elf/shell.c)
build_test_elf(bench_basic_a test_elf/bench_basic_a.c)
build_test_elf(bench_basic_b test_elf/bench_basic_b.c)
build_test_elf(framebuffer_test_a test_elf/framebuffer_test_a.c)
build_test_elf(sdl_test test_elf/sdl_test.c)
build_test_elf_lvgl(lvgl_test test_elf/lvgl_test.c)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
    COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/generate_symbols.py
            ${CMAKE_CURRENT_SOURCE_DIR}/symbols.yml
            ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/symbols.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_symbols.py
    COMMENT "Generating symbol table from symbols.yml"
    VERBATIM
)

add_custom_target(generated_symbols
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated_symbols.c
)

add_dependencies(${COMPONENT_LIB} generated_symbols)
target_link_libraries(${COMPONENT_LIB} PRIVATE m g 
	-Wl,--wrap=esp_panic_handler
	-Wl,--wrap=xt_unhandled_exception

	# heap_caps
	-Wl,--wrap=heap_caps_malloc_base
	-Wl,--wrap=heap_caps_realloc_base
	-Wl,--wrap=heap_caps_calloc_base
	-Wl,--wrap=heap_caps_malloc
	-Wl,--wrap=heap_caps_malloc_default
	-Wl,--wrap=heap_caps_realloc_default
	-Wl,--wrap=heap_caps_free
	-Wl,--wrap=heap_caps_realloc
	-Wl,--wrap=heap_caps_calloc
	-Wl,--wrap=heap_caps_aligned_alloc
	-Wl,--wrap=heap_caps_aligned_free
	-Wl,--wrap=heap_caps_aligned_calloc
	#-Wl,--wrap=heap_caps_get_total_size
	#-Wl,--wrap=heap_caps_get_free_size
	#-Wl,--wrap=heap_caps_get_minimum_free_size
	#-Wl,--wrap=heap_caps_get_largest_free_block
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
	USE_DL_PREFIX
	HAVE_MORECORE=1
	HAVE_MMAP=0
	HAVE_MREMAP=0
	MORECORE=why_sbrk
	MORECORE_CONTIGUOUS=1
	NO_MALLINFO
	malloc_getpagesize=0x10000
)
target_compile_options(${COMPONENT_LIB} PRIVATE
	-Wno-empty-body	
	-Wno-stringop-overflow
)

set(image ../fatfs_image)
fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)

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
	 "drivers/badgevms_i2c_bus.c"
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
	 ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
    PRIV_REQUIRES
	"esp_driver_ppa"
	"esp_lcd_st7703"
	"esp_psram"
	"esp_tca8418"
	"fatfs"
	"hal"
	"vfs"
    "esp_mm"
	"why_ota"
)

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

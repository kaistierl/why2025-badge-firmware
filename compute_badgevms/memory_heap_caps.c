/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dlmalloc.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "memory.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

// We need this because our version of dlmalloc is not thread safe
static SemaphoreHandle_t heap_caps_lock = NULL;

// There is no need to wrap the _prefer versions as they just call the base version
extern void  *__real_heap_caps_malloc_base(size_t size, uint32_t caps);
extern void  *__real_heap_caps_aligned_alloc_base(size_t alignment, size_t size, uint32_t caps);
extern void  *__real_heap_caps_calloc_base(size_t n, size_t size, uint32_t caps);
extern void  *__real_heap_caps_realloc_base(void *ptr, size_t size, uint32_t caps);
extern void  *__real_heap_caps_malloc(size_t size, uint32_t caps);
extern void  *__real_heap_caps_malloc_default(size_t size);
extern void  *__real_heap_caps_realloc_default(void *ptr, size_t size);
extern void   __real_heap_caps_free(void *ptr);
extern void  *__real_heap_caps_realloc(void *ptr, size_t size, uint32_t caps);
extern void  *__real_heap_caps_calloc(size_t n, size_t size, uint32_t caps);
extern void  *__real_heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
extern void   __real_heap_caps_aligned_free(void *ptr);
extern void  *__real_heap_caps_aligned_calloc(size_t alignment, size_t n, size_t size, uint32_t caps);
extern size_t __real_heap_caps_get_total_size(uint32_t caps);
extern size_t __real_heap_caps_get_free_size(uint32_t caps);
extern size_t __real_heap_caps_get_minimum_free_size(uint32_t caps);
extern size_t __real_heap_caps_get_largest_free_block(uint32_t caps);

static IRAM_ATTR esp_err_t esp_cache_get_alignment(uint32_t heap_caps, size_t *out_alignment) {
    uint32_t cache_level          = CACHE_LL_LEVEL_INT_MEM;
    uint32_t data_cache_line_size = 0;

    if (heap_caps & MALLOC_CAP_SPIRAM) {
        cache_level = CACHE_LL_LEVEL_EXT_MEM;
    }

    data_cache_line_size = cache_hal_get_cache_line_size(cache_level, CACHE_TYPE_DATA);

    *out_alignment = data_cache_line_size;

    return ESP_OK;
}

void init_memory_heap_caps() {
    heap_caps_lock = xSemaphoreCreateMutex();
}

IRAM_ATTR void *__wrap_heap_caps_malloc_base(size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *ptr = dlmalloc(size);
        xSemaphoreGive(heap_caps_lock);
        return ptr;
    }
    return __real_heap_caps_malloc_base(size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_aligned_alloc_base(size_t alignment, size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *ptr = dlmemalign(alignment, size);
        xSemaphoreGive(heap_caps_lock);
        return ptr;
    }
    return __real_heap_caps_aligned_alloc_base(alignment, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_calloc_base(size_t n, size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *ptr = dlcalloc(n, size);
        xSemaphoreGive(heap_caps_lock);
        return ptr;
    }
    return __real_heap_caps_calloc_base(n, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_realloc_base(void *ptr, size_t size, uint32_t caps) {
    if ((uintptr_t)ptr >= KERNEL_HEAP_START && (uintptr_t)ptr < SOC_EXTRAM_HIGH) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *new_ptr = dlrealloc(ptr, size);
        xSemaphoreGive(heap_caps_lock);
        return new_ptr;
    }

    if (!ptr && caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *new_ptr = dlrealloc(ptr, size);
        xSemaphoreGive(heap_caps_lock);
        return new_ptr;
    }

    return __real_heap_caps_realloc_base(ptr, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_malloc(size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *ptr = dlmalloc(size);
        xSemaphoreGive(heap_caps_lock);
        return ptr;
    }
    return __real_heap_caps_malloc(size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_malloc_default(size_t size) {
    return __real_heap_caps_malloc_default(size);
}

IRAM_ATTR void *__wrap_heap_caps_realloc_default(void *ptr, size_t size) {
    return __real_heap_caps_realloc_default(ptr, size);
}

IRAM_ATTR void __wrap_heap_caps_free(void *ptr) {
    if ((uintptr_t)ptr >= KERNEL_HEAP_START && (uintptr_t)ptr < SOC_EXTRAM_HIGH) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        dlfree(ptr);
        xSemaphoreGive(heap_caps_lock);
    } else {
        __real_heap_caps_free(ptr);
    }
}

IRAM_ATTR void *__wrap_heap_caps_realloc(void *ptr, size_t size, uint32_t caps) {
    if ((uintptr_t)ptr >= KERNEL_HEAP_START && (uintptr_t)ptr < SOC_EXTRAM_HIGH) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *new_ptr = dlrealloc(ptr, size);
        xSemaphoreGive(heap_caps_lock);
        return new_ptr;
    }

    if (!ptr && caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *new_ptr = dlrealloc(ptr, size);
        xSemaphoreGive(heap_caps_lock);
        return new_ptr;
    }

    return __real_heap_caps_realloc(ptr, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *new_ptr = dlcalloc(n, size);
        xSemaphoreGive(heap_caps_lock);
        return new_ptr;
    }
    return __real_heap_caps_calloc(n, size, caps);
}

IRAM_ATTR void *__wrap_heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *new_ptr = dlmemalign(alignment, size);
        xSemaphoreGive(heap_caps_lock);
        return new_ptr;
    }
    return __real_heap_caps_aligned_alloc(alignment, size, caps);
}

IRAM_ATTR void __wrap_heap_caps_aligned_free(void *ptr) {
    if ((uintptr_t)ptr >= KERNEL_HEAP_START && (uintptr_t)ptr < SOC_EXTRAM_HIGH) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        dlfree(ptr);
        xSemaphoreGive(heap_caps_lock);
    } else {
        __real_heap_caps_aligned_free(ptr);
    }
}

IRAM_ATTR void *__wrap_heap_caps_aligned_calloc(size_t alignment, size_t n, size_t size, uint32_t caps) {
    if (caps & MALLOC_CAP_SPIRAM) {
        xSemaphoreTake(heap_caps_lock, portMAX_DELAY);
        void *ptr = dlmemalign(alignment, size);
        xSemaphoreGive(heap_caps_lock);
        if (ptr)
            memset(ptr, 0, n * size);
        return ptr;
    }
    return __real_heap_caps_aligned_calloc(alignment, n, size, caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_total_size(uint32_t caps) {
    return __real_heap_caps_get_total_size(caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_free_size(uint32_t caps) {
    return __real_heap_caps_get_free_size(caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_minimum_free_size(uint32_t caps) {
    return __real_heap_caps_get_minimum_free_size(caps);
}

IRAM_ATTR size_t __wrap_heap_caps_get_largest_free_block(uint32_t caps) {
    return __real_heap_caps_get_largest_free_block(caps);
}

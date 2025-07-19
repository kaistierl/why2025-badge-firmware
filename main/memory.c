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

#include "memory.h"

#include "dlmalloc.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_mmu_map.h"
#include "esp_psram.h"
#include "freertos/portmacro.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "hal/cache_types.h"
#include "hal/mmu_hal.h"
#include "hal/mmu_ll.h"
#include "hal/mmu_types.h"
#include "soc/ext_mem_defs.h"
#include "soc/soc.h"
#include "task.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>

static allocator_t       page_allocator;
static allocator_t       framebuffer_allocator;
static SemaphoreHandle_t cache_lock = NULL;

typedef struct {
    uint32_t         start;   // laddr start
    uint32_t         end;     // laddr end
    size_t           size;    // region size
    cache_bus_mask_t bus_id;  // bus_id mask, for accessible cache buses
    mmu_target_t     targets; // region supported physical targets
    uint32_t         caps;    // vaddr capabilities
} mmu_mem_region_t;

extern mmu_mem_region_t const g_mmu_mem_regions[SOC_MMU_LINEAR_ADDRESS_REGION_NUM];

extern void spi_flash_enable_interrupts_caches_and_other_cpu(void);
extern void spi_flash_disable_interrupts_caches_and_other_cpu(void);

extern void spi_flash_restore_cache(uint32_t cpuid, uint32_t saved_state);
extern void spi_flash_disable_cache(uint32_t cpuid, uint32_t saved_state);

extern void        init_memory_heap_caps();
static char const *TAG = "memory";

// Copied from ESP-IDF 5.4.2 for speed
__attribute__((always_inline)) static inline bool
    why_mmu_ll_check_valid_paddr_region(uint32_t mmu_id, uint32_t paddr_start, uint32_t len) {
    int max_paddr_page_num = 0;
    if (mmu_id == MMU_LL_FLASH_MMU_ID) {
        max_paddr_page_num = SOC_MMU_FLASH_MAX_PADDR_PAGE_NUM;
    } else if (mmu_id == MMU_LL_PSRAM_MMU_ID) {
        max_paddr_page_num = SOC_MMU_PSRAM_MAX_PADDR_PAGE_NUM;
    } else {
        HAL_ASSERT(false);
    }

    return (paddr_start < (mmu_ll_get_page_size(mmu_id) * max_paddr_page_num)) &&
           (len < (mmu_ll_get_page_size(mmu_id) * max_paddr_page_num)) &&
           ((paddr_start + len - 1) < (mmu_ll_get_page_size(mmu_id) * max_paddr_page_num));
}

// Copied from ESP-IDF 5.4.2 for speed
__attribute__((always_inline)) static inline uint32_t why_mmu_hal_pages_to_bytes(uint32_t mmu_id, uint32_t page_num) {
    mmu_page_size_t page_size  = mmu_ll_get_page_size(mmu_id);
    uint32_t        shift_code = 0;
    switch (page_size) {
        case MMU_PAGE_64KB: shift_code = 16; break;
        case MMU_PAGE_32KB: shift_code = 15; break;
        case MMU_PAGE_16KB: shift_code = 14; break;
        default: HAL_ASSERT(shift_code);
    }
    return page_num << shift_code;
}

// Copied from ESP-IDF 5.4.2 for speed
__attribute__((always_inline)) static inline void
    why_mmu_hal_map_region(uint32_t mmu_id, mmu_target_t mem_type, uint32_t vaddr, uint32_t paddr, uint32_t len) {
    uint32_t page_size_in_bytes = why_mmu_hal_pages_to_bytes(mmu_id, 1);
    // HAL_ASSERT(vaddr % page_size_in_bytes == 0);
    // HAL_ASSERT(paddr % page_size_in_bytes == 0);
    // HAL_ASSERT(mmu_ll_check_valid_paddr_region(mmu_id, paddr, len));
    // HAL_ASSERT(mmu_hal_check_valid_ext_vaddr_region(mmu_id, vaddr, len, MMU_VADDR_DATA | MMU_VADDR_INSTRUCTION));

    uint32_t page_num = (len + page_size_in_bytes - 1) / page_size_in_bytes;
    uint32_t entry_id = 0;
    uint32_t mmu_val; // This is the physical address in the format that MMU supported

    mmu_val = mmu_ll_format_paddr(mmu_id, paddr, mem_type);

    while (page_num) {
        entry_id = mmu_ll_get_entry_id(mmu_id, vaddr);
        mmu_ll_write_entry(mmu_id, entry_id, mmu_val, mem_type);
        vaddr += page_size_in_bytes;
        mmu_val++;
        page_num--;
    }
}

__attribute__((always_inline)) static inline void invalidate_caches(uintptr_t vaddr_start, uint32_t len) {
    cache_ll_l1_invalidate_icache_addr(CACHE_LL_ID_ALL, vaddr_start, len);
    cache_ll_l1_invalidate_dcache_addr(CACHE_LL_ID_ALL, vaddr_start, len);
    cache_ll_l2_invalidate_cache_addr(CACHE_LL_ID_ALL, vaddr_start, len);
}

__attribute__((always_inline)) static inline void writeback_caches(uintptr_t vaddr_start, uint32_t len) {
    // cache_ll_writeback_all(CACHE_LL_LEVEL_ALL, CACHE_TYPE_DATA, CACHE_LL_ID_ALL);
    cache_ll_writeback_addr(CACHE_LL_LEVEL_ALL, CACHE_TYPE_DATA, CACHE_LL_ID_ALL, vaddr_start, len);
}

IRAM_ATTR esp_err_t __wrap_esp_cache_msync(void *addr, size_t size, int flags) {
    spi_flash_disable_interrupts_caches_and_other_cpu();
    if (flags & ESP_CACHE_MSYNC_FLAG_DIR_C2M) {
        writeback_caches((uintptr_t)addr, size);
        spi_flash_enable_interrupts_caches_and_other_cpu();
        return 0;
    }

    if (flags & ESP_CACHE_MSYNC_FLAG_DIR_M2C) {
        invalidate_caches((uintptr_t)addr, size);
        spi_flash_enable_interrupts_caches_and_other_cpu();
        return 0;
    }

    writeback_caches((uintptr_t)addr, size);
    invalidate_caches((uintptr_t)addr, size);

    spi_flash_enable_interrupts_caches_and_other_cpu();
    return 0;
}

__attribute__((always_inline)) static inline void
    why_mmu_hal_unmap_region(uint32_t mmu_id, uint32_t vaddr, uint32_t len) {
    uint32_t page_size_in_bytes = why_mmu_hal_pages_to_bytes(mmu_id, 1); // Already inline
    // HAL_ASSERT(vaddr % page_size_in_bytes == 0);
    // HAL_ASSERT(mmu_hal_check_valid_ext_vaddr_region(mmu_id, vaddr, len, MMU_VADDR_DATA | MMU_VADDR_INSTRUCTION));

    uint32_t page_num = (len + page_size_in_bytes - 1) / page_size_in_bytes;
    uint32_t entry_id = 0;
    while (page_num) {
        entry_id = mmu_ll_get_entry_id(mmu_id, vaddr);
        mmu_ll_set_entry_invalid(mmu_id, entry_id);
        vaddr += page_size_in_bytes;
        page_num--;
    }
}

__attribute__((always_inline)) static inline void
    map_regions(allocation_range_t *head_range, allocation_range_t *tail_range) {
    uint32_t            mmu_id     = mmu_hal_get_id_from_target(MMU_TARGET_PSRAM0);
    uintptr_t           start      = tail_range->vaddr_start;
    uintptr_t           total_size = 0;
    allocation_range_t *r          = head_range;
    if (!r)
        return;

    while (r) {
        ESP_DRAM_LOGV(
            DRAM_STR("map_regions"),
            "Mapping region ptr %p, vaddr_start: %p, paddr_start %p, size %u, r->next = %p",
            r,
            (void *)r->vaddr_start,
            (void *)r->paddr_start,
            r->size,
            r->next
        );
        spi_flash_disable_interrupts_caches_and_other_cpu();
        why_mmu_hal_map_region(mmu_id, MMU_TARGET_PSRAM0, r->vaddr_start, r->paddr_start, r->size);
        // Give other tasks a chance
        spi_flash_enable_interrupts_caches_and_other_cpu();
        total_size += r->size;
        r           = r->next;
    }

    // Invalidate all caches at once
    spi_flash_disable_interrupts_caches_and_other_cpu();
    invalidate_caches(start, total_size);
    spi_flash_enable_interrupts_caches_and_other_cpu();
}

void IRAM_ATTR remap_task(task_info_t *task_info) {
    uint32_t mmu_id = mmu_hal_get_id_from_target(MMU_TARGET_PSRAM0);

    allocation_range_t *r = task_info->allocations;

    while (r) {
        why_mmu_hal_map_region(mmu_id, MMU_TARGET_PSRAM0, r->vaddr_start, r->paddr_start, r->size);
        r = r->next;
    }

    // Invalidate all caches at once
    invalidate_caches(task_info->heap_start, task_info->heap_size);
}

void IRAM_ATTR unmap_task(task_info_t *task_info) {
    uint32_t mmu_id = mmu_hal_get_id_from_target(MMU_TARGET_PSRAM0);

    allocation_range_t *r = task_info->allocations;

    // Ensure we wrote back all caches
    writeback_caches(task_info->heap_start, task_info->heap_size);

    while (r) {
        why_mmu_hal_unmap_region(mmu_id, r->vaddr_start, r->size);
        r = r->next;
    }
}

IRAM_ATTR void pages_deallocate(allocation_range_t *head_range) {
    allocation_range_t *r = head_range;
    while (r) {
        ESP_LOGI(
            TAG,
            "Deallocating page. vaddr_start = %p, paddr_start = %p, size = %zi",
            (void *)r->vaddr_start,
            (void *)r->paddr_start,
            r->size
        );
        page_deallocate(r->paddr_start);
        allocation_range_t *n = r->next;
        free(r);
        r = n;
    }
}

IRAM_ATTR bool pages_allocate(
    uintptr_t vaddr_start, uintptr_t pages, allocation_range_t **head_range, allocation_range_t **tail_range
) {
    if (pages > buddy_get_free_pages(&page_allocator)) {
        return false;
    }

    uint32_t to_allocate   = pages;
    uint32_t allocate_size = pages;

    // Since we don't know how much contiguous free pages are available we will
    // have to build up a list of pages to allocate. We want to keep the section with
    // interrupts disabled as short as possible so do all of the work beforehand.
    //
    // We want to have as few mappings as possible, so just allocating 1 at a time is
    // not an option
    while (to_allocate) {
        uintptr_t new_page = 0;
        allocate_size      = allocate_size > to_allocate ? to_allocate : allocate_size;
        ESP_LOGI(TAG, "Attempting allocation of size %li pages\n", allocate_size);

        allocation_range_t *new_range = malloc(sizeof(allocation_range_t));
        if (new_range) {
            new_page = page_allocate(allocate_size * SOC_MMU_PAGE_SIZE);
        }

        if (!new_page) {
            if (allocate_size == 1) {
                // No more memory
                allocation_range_t *r = *head_range;
                while (r) {
                    allocation_range_t *n = r->next;
                    page_deallocate(r->paddr_start);
                    free(r);
                    r = n;
                }
                return false;
            }
            // Try again
            allocate_size -= 1;
            continue;
        }

        ESP_LOGI(TAG, "Got new page at address %p", (void *)new_page);
        // We are the first allocation
        if (!*tail_range) {
            *tail_range = new_range;
        }
        new_range->vaddr_start = vaddr_start;
        // The allocator doesn't know the physical ranges
        new_range->paddr_start = new_page;
        new_range->size        = allocate_size * SOC_MMU_PAGE_SIZE;
        new_range->next        = *head_range;

        // We are the new head
        *head_range  = new_range;
        // Next allocation will be immediately after us in vaddr
        vaddr_start += allocate_size * SOC_MMU_PAGE_SIZE;
        to_allocate -= allocate_size;

        ESP_LOGI(
            TAG,
            "New range: vaddr_start = %p, paddr_start = %p, size = %zi",
            (void *)new_range->vaddr_start,
            (void *)new_range->paddr_start,
            new_range->size
        );
    }

    return true;
}

uintptr_t IRAM_ATTR framebuffer_vaddr_allocate(size_t size, size_t *out_pages) {
    size_t aligned_size = ((size + (SOC_MMU_PAGE_SIZE - 1)) & ~(SOC_MMU_PAGE_SIZE - 1));
    void  *ret          = buddy_allocate(&framebuffer_allocator, aligned_size, 0, 0);
    if (ret) {
        *out_pages = aligned_size / SOC_MMU_PAGE_SIZE;
        return (uintptr_t)ret;
    }

    *out_pages = 0;
    return 0;
}

void IRAM_ATTR framebuffer_vaddr_deallocate(uintptr_t start_address) {
    if (start_address) {
        buddy_deallocate(&framebuffer_allocator, (void *)start_address);
    }
}

void IRAM_ATTR framebuffer_map_pages(allocation_range_t *head_range, allocation_range_t *tail_range) {
    map_regions(head_range, tail_range);
}

void IRAM_ATTR NOINLINE_ATTR *why_sbrk(intptr_t increment) {
    task_info_t *task_info = get_task_info();
    uintptr_t    old       = task_info->heap_end;
    ESP_LOGI("sbrk", "Calling sbrk(%zi) from task %d", increment, task_info->pid);

    if (!increment)
        goto out;

    if (increment > 0) {
        // Allocating new pages
        uintptr_t vaddr_start = task_info->heap_end;

        if (vaddr_start + increment > SOC_EXTRAM_HIGH) {
            goto error;
        }

        allocation_range_t *head_range = NULL;
        allocation_range_t *tail_range = NULL;

        uint32_t pages = increment / SOC_MMU_PAGE_SIZE;

        // Ranges are in reverse order, when we insert our new ranges into
        // the task_info this range needs to be tied to the old head

        if (!pages_allocate(vaddr_start, pages, &head_range, &tail_range)) {
            goto error;
        }

        // Actually map our new memory
        ESP_LOGI(
            TAG,
            "Allocating new ranges for pid %i, head: %p, tail %p, allocations %p",
            task_info->pid,
            head_range,
            tail_range,
            task_info->allocations
        );

        map_regions(head_range, tail_range);

        tail_range->next       = task_info->allocations;
        task_info->allocations = head_range;

        task_info->heap_size += increment;
        task_info->heap_end  += increment;
    } else {
        // increment is negative
        int32_t  decrement_amount = task_info->heap_size + increment;
        int32_t  to_decrement     = decrement_amount;
        uint32_t mmu_id           = mmu_hal_get_id_from_target(MMU_TARGET_PSRAM0);

        allocation_range_t *r = task_info->allocations;
        while (r && to_decrement) {
            // We always free from the end
            if (r->size <= to_decrement) {
                // Current block is smaller than we want to decrement
                ESP_LOGI(
                    TAG,
                    "Deallocating whole page. vaddr_start = %p, paddr_start = %p, size = %zi",
                    (void *)r->vaddr_start,
                    (void *)r->paddr_start,
                    r->size
                );
                spi_flash_disable_interrupts_caches_and_other_cpu();
                why_mmu_hal_unmap_region(mmu_id, r->vaddr_start, r->size);
                spi_flash_enable_interrupts_caches_and_other_cpu();

                page_deallocate(r->paddr_start);

                to_decrement          -= r->size;
                allocation_range_t *n  = r->next;
                free(r);
                task_info->allocations = n;
                r                      = n;
            } else {
                // Current block is larger than we want to decrement
                ESP_LOGI(
                    TAG,
                    "Deallocating partial. vaddr_start = %p, paddr_start = %p, size = %zi, removing %li",
                    (void *)r->vaddr_start,
                    (void *)r->paddr_start,
                    r->size,
                    to_decrement
                );

                spi_flash_disable_interrupts_caches_and_other_cpu();
                // Make sure that all existing ram is written
                writeback_caches(r->vaddr_start, r->size - to_decrement);
                // Unmap the whole region
                why_mmu_hal_unmap_region(mmu_id, r->vaddr_start, r->size);
                // Remap the smaller region
                r->size -= to_decrement;
                why_mmu_hal_map_region(mmu_id, MMU_TARGET_PSRAM0, r->vaddr_start, r->paddr_start, r->size);
                invalidate_caches(r->vaddr_start, r->size);
                spi_flash_enable_interrupts_caches_and_other_cpu();

                page_deallocate(r->paddr_start + to_decrement);
                to_decrement = 0;
            }
        }
        task_info->heap_size -= decrement_amount;
        task_info->heap_end  -= decrement_amount;
    }

out:
    ESP_LOGI(
        "sbrk",
        "Calling sbrk(%zi) from task %d, returning %p, heap_size=%zi, heap_end=%p",
        increment,
        task_info->pid,
        (void *)old,
        task_info->heap_size,
        (void *)task_info->heap_end
    );
    return (void *)old;

error:
    ESP_LOGW(TAG, "Out of memory for task %i", task_info->pid);
    task_info->_errno = ENOMEM;
    return (void *)-1;
}

void page_deallocate(uintptr_t paddr_start) {
    buddy_deallocate(&page_allocator, (void *)PADDR_TO_ADDR(paddr_start));
}

uintptr_t page_allocate(size_t size) {
    void *ret = buddy_allocate(&page_allocator, size, 0, 0);
    if (ret) {
        return ADDR_TO_PADDR((uintptr_t)ret);
    }
    // This is fine because page 0 is always used by the allocator itself
    return 0;
}

// Cribbed from ESP-IDF 5.4.2
static IRAM_ATTR bool test_psram(intptr_t v_start, size_t size) {
    int volatile *spiram = (int volatile *)v_start;
    size_t        p;
    int           errct       = 0;
    int           initial_err = -1;

    ESP_DRAM_LOGW(DRAM_STR("memory_test"), "Starting write");
    for (p = 0; p < (size / sizeof(int)); p += 8) {
        spiram[p] = p ^ 0xAAAAAAAA;
    }

    ESP_DRAM_LOGW(DRAM_STR("memory_test"), "Writing back pages");
    writeback_caches(v_start, size);
    ESP_DRAM_LOGW(DRAM_STR("memory_test"), "Invalidate pages");
    invalidate_caches(v_start, size);

    ESP_DRAM_LOGW(DRAM_STR("memory_test"), "Starting read");
    for (p = 0; p < (size / sizeof(int)); p += 8) {
        if (spiram[p] != (p ^ 0xAAAAAAAA)) {
            errct++;
            if (errct == 1) {
                initial_err = p * 4;
            }
        }
    }
    if (errct) {
        ESP_DRAM_LOGE(
            DRAM_STR("memory_test"),
            "SPI SRAM memory test fail. %d/%d writes failed, first @ %X",
            errct,
            size / 32,
            initial_err + v_start
        );
        return false;
    } else {
        ESP_DRAM_LOGI(DRAM_STR("memory_test"), "SPI SRAM memory test OK");
        return true;
    }
}

uint32_t vaddr_to_paddr(uint32_t vaddr) {
    uint32_t mmu_id = mmu_hal_get_id_from_target(MMU_TARGET_PSRAM0);

    uint32_t paddr;
    mmu_target_t target;
    mmu_hal_vaddr_to_paddr(mmu_id, vaddr, &paddr, &target);

    return paddr;
}

void IRAM_ATTR memory_init() {
    for (int i = 0; i < SOC_MMU_LINEAR_ADDRESS_REGION_NUM; i++) {
        ESP_LOGI(TAG, "MMU Region  %u", i);
        ESP_LOGI(TAG, "  start   : %p", (void *)g_mmu_mem_regions[i].start);
        ESP_LOGI(TAG, "  size    : %u", g_mmu_mem_regions[i].size);
        ESP_LOGI(TAG, "  targets : %u", g_mmu_mem_regions[i].targets);
    }

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Initializing PSRAM");
    esp_psram_init();

    size_t psram_size = esp_psram_get_size();

    uint32_t mmu_id = mmu_hal_get_id_from_target(MMU_TARGET_PSRAM0);
    uint32_t out_len;

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Disabling caches and interrupts");
    spi_flash_disable_interrupts_caches_and_other_cpu();

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Unmapping all of our address space");
    mmu_ll_unmap_all(mmu_id);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Mapping all pages for memory test");
    mmu_hal_map_region(mmu_id, MMU_TARGET_PSRAM0, VADDR_START, 0x0, psram_size, &out_len);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Invalidate all pages for memory test");
    invalidate_caches(VADDR_START, out_len);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Running memory test");
    if (!test_psram(VADDR_START, out_len)) {
        spi_flash_enable_interrupts_caches_and_other_cpu();
        esp_restart();
    }

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Unmapping all of our address space");
    mmu_ll_unmap_all(mmu_id);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Map allocator address space");
    mmu_hal_map_region(mmu_id, MMU_TARGET_PSRAM0, VADDR_START, 0, SOC_MMU_PAGE_SIZE, &out_len);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Invalidate allocator address space");
    invalidate_caches(VADDR_START, out_len);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Re-enabling caches and interrupts");
    spi_flash_enable_interrupts_caches_and_other_cpu();

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Initialzing memory pool");
    init_pool(&page_allocator, (void *)VADDR_START, (void *)VADDR_START + psram_size, 0);

    uintptr_t framebuffer_page = page_allocate(SOC_MMU_PAGE_SIZE);

    uintptr_t *tmp = calloc(1, sizeof(uintptr_t) * 512);

    for (int i = 0; i < 510; ++i) {
        uintptr_t t = page_allocate(1);
        if (t == 0x00070000) {
            ESP_LOGE(TAG, "reserved page 0x70000");
            break;
        }

        tmp[i] = t;
    }

    for (int i = 0; i < 510; ++i) {
        if (! tmp[i]) break;
        page_deallocate(tmp[i]);
    }

    free(tmp);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Disabling caches and interrupts");
    spi_flash_disable_interrupts_caches_and_other_cpu();

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Map framebuffer allocator address space");
    mmu_hal_map_region(
        mmu_id,
        MMU_TARGET_PSRAM0,
        FRAMEBUFFER_HEAP_START,
        framebuffer_page,
        SOC_MMU_PAGE_SIZE,
        &out_len
    );

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Invalidate allocator address space");
    invalidate_caches(FRAMEBUFFER_HEAP_START, out_len);

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Re-enabling caches and interrupts");
    spi_flash_enable_interrupts_caches_and_other_cpu();

    ESP_DRAM_LOGW(DRAM_STR("memory_init"), "Initialzing framebuffer pool");
    init_pool(
        &framebuffer_allocator,
        (void *)FRAMEBUFFER_HEAP_START,
        (void *)FRAMEBUFFER_HEAP_START + FRAMEBUFFER_HEAP_SIZE,
        0
    );

    cache_lock = xSemaphoreCreateMutex();
    init_memory_heap_caps();
    print_allocator(&page_allocator);
}

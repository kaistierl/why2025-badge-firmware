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

#pragma once

#include "buddy_alloc.h"
#include "dlmalloc.h"
#include "esp_log.h"
#include "soc/soc.h"

/* BadgeVMS memory map for extram
 *
 * SOC_EXTRAM_LOW
 * ...                      Page allocator metadata
 * SOC_EXTRAM_LOW + 1 page
 * ...                      Kernel SPIRAM heap
 * SOC_EXTRAM_LOW + 5MB
 * ...                      Framebuffers
 * SOC_EXTRAM_LOW + 20MB
 * ...                      Unused
 * SOC_EXTRAM_LOW + 32MB - 1 page
 * ...                      Guard page
 * SOC_EXTRAM_LOW + 32MB
 * ...                      User applications
 * SOC_EXTRAM_HIGH
 */

#define PHYSMEM_AMOUNT    (32 * 1024 * 1024)
#define VADDR_START       SOC_EXTRAM_LOW
#define VADDR_TASK_START  ((SOC_EXTRAM_LOW + PHYSMEM_AMOUNT) & ~(SOC_MMU_PAGE_SIZE - 1))
#define VADDR_TASK_END    SOC_EXTRAM_HIGH
// Allocate 10MB of VADDR space for the kernel
#define KERNEL_HEAP_SIZE  (((1024 * 1024) * 5) - SOC_MMU_PAGE_SIZE)
#define KERNEL_HEAP_START SOC_EXTRAM_LOW + SOC_MMU_PAGE_SIZE

// Allocate 10MB of VADDR space for framebuffers
#define FRAMEBUFFER_HEAP_SIZE  (1024 * 1024 * 15)
#define FRAMEBUFFER_HEAP_START ((SOC_EXTRAM_LOW + FRAMEBUFFER_HEAP_SIZE) & ~(SOC_MMU_PAGE_SIZE - 1))
#define FRAMEBUFFERS_START     FRAMEBUFFFER_HEAP_START + SOC_MMU_PAGE_SIZE

#define ADDR_TO_PADDR(a) (a - VADDR_START)
#define PADDR_TO_ADDR(a) (a + VADDR_START)

#if ((KERNEL_HEAP_START + KERNEL_HEAP_SIZE) > VADDR_TASK_START)
#error "Kernel Heap overlaps with largest possible user program"
#endif

typedef struct allocation_range_s {
    uintptr_t                  vaddr_start;
    uintptr_t                  paddr_start;
    size_t                     size;
    struct allocation_range_s *next;
} allocation_range_t;

typedef struct task_info task_info_t;

void     *why_sbrk(intptr_t increment);
void      page_deallocate(uintptr_t paddr_start);
uintptr_t page_allocate(size_t size);

bool pages_allocate(
    uintptr_t vaddr_start, uintptr_t pages, allocation_range_t **head_range, allocation_range_t **tail_range
);
void pages_deallocate(allocation_range_t *head_range);

uintptr_t framebuffer_vaddr_allocate(size_t size, size_t *out_pages);
void      framebuffer_vaddr_deallocate(uintptr_t start_address);
void      framebuffer_map_pages(allocation_range_t *head_range, allocation_range_t *tail_range);

void memory_init();
void dump_mmu();

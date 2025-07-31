// SPDX-License-Identifier: MIT

/* Buddy Allocator for BadgerOS and BadgeVMS
 * Adapted by the orignal author for BadgeVMS
 */

#pragma once

#include "esp_attr.h"
#include "freertos/FreeRTOS.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE        SOC_MMU_PAGE_SIZE
#define MAX_MEMORY_POOLS 2

#define ALIGN_UP(x, y)   (void *)(((size_t)(x) + (y - 1)) & ~(y - 1))
#define ALIGN_DOWN(x, y) (void *)((size_t)(x) & ~(y - 1))

#define ALIGN_PAGE_UP(x)   ALIGN_UP(x, PAGE_SIZE)
#define ALIGN_PAGE_DOWN(x) ALIGN_DOWN(x, PAGE_SIZE)

enum block_type { BLOCK_TYPE_FREE, BLOCK_TYPE_USER, BLOCK_TYPE_PAGE, BLOCK_TYPE_ERROR };

typedef struct buddy_block {
    uint8_t             pid;
    uint8_t             order;
    bool                in_list;
    bool                is_waste;
    enum block_type     type;
    struct buddy_block *next;
    struct buddy_block *prev;
} buddy_block_t;

typedef struct {
    uint32_t       flags;
    void          *start;
    void          *end;
    void          *pages_start;
    void          *pages_end;
    size_t         pages;
    size_t         free_pages;
    uint8_t        max_order;
    uint8_t        max_order_free;
    uint32_t       max_order_waste;
    buddy_block_t  waste_list;
    buddy_block_t *free_lists;
    buddy_block_t *blocks;
} memory_pool_t;

typedef struct {
    uint8_t           memory_pool_num;
    memory_pool_t     memory_pools[MAX_MEMORY_POOLS];
    SemaphoreHandle_t memory_pool_mutex;
} allocator_t;

void init_pool(allocator_t *allocator, void *mem_start, void *mem_end, uint32_t flags);
void print_allocator(allocator_t *allocator);

void  *buddy_allocate(allocator_t *allocator, size_t size, enum block_type type, uint32_t flags);
// void           *buddy_reallocate(void *ptr, size_t size);
void   buddy_deallocate(allocator_t *allocator, void *ptr);
// void            buddy_split_allocated(void *ptr);
// enum block_type buddy_get_type(void *ptr);
// size_t          buddy_get_size(void *ptr);
size_t buddy_get_free_pages(allocator_t *allocator);
size_t buddy_get_total_pages(allocator_t *allocator);

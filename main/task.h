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

#include "device.h"
#include "dlmalloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "khash.h"
#include "memory.h"

#include <stdatomic.h>
#include <stdbool.h>

#include <time.h> // For task_info_t

KHASH_MAP_INIT_INT(ptable, void *);
KHASH_MAP_INIT_INT(restable, int);

#define MAXFD           128
#define STRERROR_BUFLEN 128
#define NUM_PIDS        128
#define MAX_PID         127
#define MIN_STACK_SIZE  8192

#define TASK_PRIORITY            5
#define TASK_PRIORITY_FOREGROUND 6

typedef enum { RES_ICONV_OPEN, RES_REGCOMP, RES_OPEN, RES_RESOURCE_TYPE_MAX } task_resource_type_t;

typedef enum {
    TASK_TYPE_ELF,
    TASK_TYPE_ELF_ROM,
} task_type_t;

typedef struct {
    bool      is_open;
    int       dev_fd;
    device_t *device;
} file_handle_t;

typedef struct task_info_psram {
    // Buffers
    file_handle_t file_handles[MAXFD];
    char          strerror_buf[STRERROR_BUFLEN];
    char          asctime_buf[26];
    char          ctime_buf[26];

    // Structured
    struct tm            gmtime_tm;
    struct tm            localtime_tm;
    struct malloc_state  malloc_state;
    struct malloc_params malloc_params;
} task_info_psram_t;

typedef struct task_info {
    // Pointers
    TaskHandle_t handle;
    khash_t(restable) * resources[RES_RESOURCE_TYPE_MAX];
    allocation_range_t *allocations;
    void               *data;
    char               *buffer;
    char              **argv;
    char              **argv_back;
    char               *strtok_saveptr;
    uintptr_t           heap_start;
    uintptr_t           heap_end;
    void (*task_entry)(struct task_info *task_info);

    // Small variables
    bool         buffer_in_rom;
    pid_t        pid;
    int          argc;
    int          _errno;
    task_type_t  type;
    size_t       heap_size;
    size_t       argv_size;
    size_t       max_memory;
    size_t       current_memory;
    size_t       max_files;
    size_t       current_files;
    size_t       stack_size;
    unsigned int seed;

    task_info_psram_t *psram;
    void              *pad; // For debugging
} task_info_t;

__attribute__((always_inline)) inline static task_info_t *get_task_info() {
    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(NULL, 0);

    return task_info;
}

void         task_init();
pid_t        run_task(void *buffer, int stack_size, task_type_t type, int argc, char *argv[]);
void         task_record_resource_alloc(task_resource_type_t type, void *ptr);
void         task_record_resource_free(task_resource_type_t type, void *ptr);
uint32_t     get_num_tasks();
task_info_t *get_taskinfo_for_pid(pid_t pid);

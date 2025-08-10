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

#include "badgevms/device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "memory.h"
#include "thirdparty/dlmalloc.h"

#include <stdatomic.h>
#include <stdbool.h>

#include <time.h> // For task_info_t

#define MAXFD           128
#define STRERROR_BUFLEN 128
#define NUM_PIDS        128
#define MAX_PID         127

#define TASK_PRIORITY_LOW        4
#define TASK_PRIORITY            5
#define TASK_PRIORITY_FOREGROUND 6

typedef struct kh_restable_s kh_restable_t;

typedef enum {
    RES_ICONV_OPEN,
    RES_REGCOMP,
    RES_OPEN,
    RES_OTA,
    RES_WINDOW,
    RES_DEVICE,
    RES_ESP_TLS,
    RES_RESOURCE_TYPE_MAX
} task_resource_type_t;

typedef enum {
    TASK_TYPE_ELF,
    TASK_TYPE_ELF_PATH,
    TASK_TYPE_THREAD,
} task_type_t;

typedef struct {
    bool      is_open;
    int       dev_fd;
    device_t *device;
} file_handle_t;

typedef struct {
    allocation_range_t  *pages;
    uintptr_t            start;
    uintptr_t            end;
    size_t               size;
    atomic_int           refcount;
    size_t               max_memory;
    size_t               max_files;
    size_t               current_files;
    file_handle_t        file_handles[MAXFD];
    struct malloc_state  malloc_state;
    struct malloc_params malloc_params;
    kh_restable_t       *resources[RES_RESOURCE_TYPE_MAX];
} task_thread_t;

typedef struct task_info {
    // Pointers
    TaskHandle_t   handle;
    task_thread_t *thread;
    void const    *buffer;
    void          *data;
    char          *file_path;
    char         **argv;
    char         **argv_back;
    char          *strtok_saveptr;
    char          *application_uid;
    uint16_t       stack_size;
    void (*task_entry)(struct task_info *task_info);
    void (*thread_entry)(void *user_data);

    // Small variables
    pid_t        pid;
    pid_t        parent;
    int          argc;
    int          _errno;
    task_type_t  type;
    size_t       argv_size;
    unsigned int seed;

    // Buffers
    char strerror_buf[STRERROR_BUFLEN];
    char asctime_buf[26];
    char ctime_buf[26];

    // Structured
    struct tm     gmtime_tm;
    struct tm     localtime_tm;
    QueueHandle_t children;

    void *pad; // For debugging
} task_info_t;

extern task_info_t kernel_task;

__attribute__((always_inline)) inline static task_info_t *get_task_info() {
    if (xTaskGetApplicationTaskTag(NULL) == (void *)0x12345678) {
        return pvTaskGetThreadLocalStoragePointer(NULL, 1);
    }
    return &kernel_task;
}

bool         task_init();
pid_t        run_task(void const *buffer, uint16_t stack_size, task_type_t type, int argc, char *argv[]);
pid_t        run_task_path(char const *path, uint16_t stack_size, task_type_t type, int argc, char *argv[]);
void         task_record_resource_alloc(task_resource_type_t type, void *ptr);
void         task_record_resource_free(task_resource_type_t type, void *ptr);
void         task_set_application_uid(pid_t pid, char const *unique_id);
bool         task_application_is_running(char const *unique_id);
uint32_t     get_num_tasks();
task_info_t *get_taskinfo_for_pid(pid_t pid);

BaseType_t create_kernel_task(
    TaskFunction_t      pvTaskCode,
    char const *const   pcName,
    uint32_t const      usStackDepth,
    void *const         pvParameters,
    UBaseType_t         uxPriority,
    TaskHandle_t *const pvCreatedTask,
    BaseType_t const    xCoreID
);

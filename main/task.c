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

#include "task.h"

#include "bitfuncs.h"
#include "compositor.h"
#include "esp_elf.h"
#include "esp_log.h"
#include "event.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "hal/cache_types.h"
#include "hal/mmu_hal.h"
#include "hal/mmu_ll.h"
#include "hal/mmu_types.h"
#include "hash_helper.h"
#include "khash.h"
#include "memory.h"
#include "why_io.h"
#include "ota.h"

#include <stdatomic.h>

#include <iconv.h>
#include <regex.h>
#include <string.h>

extern void writeback_and_invalidate_task(task_info_t *task_info);
extern void remap_task(task_info_t *task_info);
extern void unmap_task(task_info_t *task_info);
extern void __real_xt_unhandled_exception(void *frame);

static char const *TAG = "task";

IRAM_ATTR task_info_t kernel_task = {
    .heap_start = KERNEL_HEAP_START,
    .heap_end   = KERNEL_HEAP_START,
};

typedef struct {
    TaskFunction_t entry;
    void          *pvParameters;
} kernel_task_param_t;

static uint32_t num_tasks = 0;

static task_info_t      *process_table[NUM_PIDS];
static SemaphoreHandle_t process_table_lock = NULL;

static TaskHandle_t  hades_handle;
static QueueHandle_t hades_queue;

static TaskHandle_t  zeus_handle;
static QueueHandle_t zeus_queue;

static SemaphoreHandle_t pid_table_lock = NULL;
static pid_t             pid_table[NUM_PIDS];
static uint32_t          head = 0;
static uint32_t          tail = MAX_PID;

static pid_t pid_allocate() {
    if (xSemaphoreTake(pid_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    pid_t pid = pid_table[head];
    if (pid == -1) {
        goto out;
    }
    pid_table[head] = -1;
    head            = (head + 1) % NUM_PIDS;

out:
    xSemaphoreGive(pid_table_lock);
    return pid;
}

static void pid_free(pid_t pid) {
    if (pid < 0) {
        return;
    }

    if (xSemaphoreTake(pid_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    pid_table[tail] = pid;
    tail            = (tail + 1) % NUM_PIDS;

    xSemaphoreGive(pid_table_lock);
}

task_info_t *get_taskinfo_for_pid(pid_t pid) {
    return process_table[pid];
}

static void process_table_add_task(task_info_t *task_info) {
    if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get process table mutex");
        abort();
    }

    process_table[task_info->pid] = task_info;

    xSemaphoreGive(process_table_lock);
}

static void process_table_remove_task(task_info_t *task_info) {
    if (xSemaphoreTake(process_table_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get process table mutex");
        abort();
    }

    process_table[task_info->pid] = NULL;

    xSemaphoreGive(process_table_lock);
}

void vTaskPreDeletionHook(TaskHandle_t handle) {
    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(handle, 0);

    if (!task_info) {
        ESP_DRAM_LOGW(DRAM_STR("vTaskPreDeletionHook"), "Called for a kernel task");
        return;
    }

    pid_t pid = task_info->pid;

    if (xPortInIsrContext()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendFromISR(hades_queue, &pid, &xHigherPriorityTaskWoken) == errQUEUE_FULL) {
            ESP_DRAM_LOGW(DRAM_STR("vTaskPreDeletionHook"), "Hades queue full, memory leak.");
        }

    } else {
        xQueueSend(hades_queue, &pid, portMAX_DELAY);
    }
}

static task_info_t *task_info_init() {
    task_info_t *task_info = heap_caps_calloc(1, sizeof(task_info_t), MALLOC_CAP_SPIRAM);
    if (!task_info) {
        ESP_LOGE(TAG, "Out of memory trying to allocate task info");
        return NULL;
    }

    task_info->current_files           = 3;
    task_info->file_handles[0].is_open = true;
    task_info->file_handles[0].device  = device_get("TT01");
    task_info->file_handles[1].is_open = true;
    task_info->file_handles[1].device  = device_get("TT01");
    task_info->file_handles[2].is_open = true;
    task_info->file_handles[2].device  = device_get("TT01");

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        task_info->resources[i] = kh_init(restable);
    }

    return task_info;
}

static void task_info_delete(task_info_t *task_info) {
    if (!task_info) {
        return;
    }

    ESP_LOGI(TAG, "Deleting task_info %pi for pid %i", task_info, task_info->pid);

    for (int i = 0; i < MAXFD; ++i) {
        // We sadly can't reuse the why_close code as it must be ran from inside the user task
        if (task_info->file_handles[i].is_open) {
            ESP_LOGI(TAG, "Cleaning up open filehandle %i", i);
            if (task_info->file_handles[i].device->_close) {
                task_info->file_handles[i].device->_close(
                    task_info->file_handles[i].device,
                    task_info->file_handles[i].dev_fd
                );
            }
        }
    }

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        for (khiter_t k = kh_begin(task_info->resources[i]); k != kh_end(task_info->resources[i]); ++k) {
            if (kh_exist(task_info->resources[i], k)) {
                void                *ptr  = (void *)kh_key(task_info->resources[i], k);
                task_resource_type_t type = kh_value(task_info->resources[i], k);

                switch (type) {
                    case RES_ICONV_OPEN:
                        ESP_LOGI(TAG, "Cleaning up iconv %p", ptr);
                        iconv_close(ptr);
                        break;
                    case RES_REGCOMP:
                        ESP_LOGI(TAG, "Cleaning up regcomp %p", ptr);
                        regfree(ptr);
                        break;
                    case RES_OPEN:
                        // Already handled above
                        break;
                    case RES_WINDOW:
                        ESP_LOGW(TAG, "Cleaning up window %p", ptr);
                        window_destroy(ptr);
                        break;
                    case RES_DEVICE:
                        device_t *dev = (device_t *)ptr;
                        if (dev->_destroy) {
                            dev->_destroy(ptr);
                        }
                        break;
                    case RES_OTA: ota_session_abort(ptr); break;
                    default: ESP_LOGE(TAG, "Unknown resource type %i in task_info_delete", type);
                }
            }
        }

        kh_destroy(restable, task_info->resources[i]);
    }

    free(task_info->file_path);
    free(task_info->argv_back);
    heap_caps_free(task_info);
    ESP_LOGW(TAG, "Cleaned up task");
}

// Try and handle misbehaving tasks
void IRAM_ATTR cerberos() {
    if (xPortInIsrContext()) {
        ESP_DRAM_LOGE("CERBEROS", "In ISR context, no idea what to do");

    } else {
        task_info_t *task_info = get_task_info();
        if (!task_info || !task_info->pid) {
            ESP_LOGE("CERBEROS", "Trying to kill kernel. This is probably bad");
            return;
        }

        ESP_LOGW("CERBEROS", "Taking task %u to Hades, Woof", task_info->pid);
        vTaskDelete(NULL);
    }
}

void IRAM_ATTR __wrap_xt_unhandled_exception(void *frame) {
#if 0
    task_info_t *task_info = get_task_info();
    if (task_info && task_info->pid) {
        esp_rom_printf("Task %u caused an unhandled exception, Cerberos will deal with it\n", task_info->pid);
        __asm__ volatile("csrw mepc, %0\n\t" // Set return address
                         "mret\n\t"
                         :
                         : "r"(cerberos)
                         : "t0", "memory");
    }
#endif
    __real_xt_unhandled_exception(frame);
}

static void elf_task(task_info_t *task_info) {
    int ret;

    esp_elf_t *elf = calloc(1, sizeof(esp_elf_t));
    if (!elf) {
        ESP_LOGE(TAG, "Out of memory trying to allocate elf structure");
        return;
    }
    task_info->data = elf;

    uint32_t vmem = why_elf_get_vmem_requirements((uint8_t const *)task_info->buffer);

    ESP_LOGI(TAG, "VMEM requirement: %lu\n", vmem);

    ret = esp_elf_init(elf);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to initialize ELF file errno=%d", ret);
        return;
    }

    ret = esp_elf_relocate(elf, (uint8_t const *)task_info->buffer);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to relocate ELF file errno=%d", ret);
        goto out;
    }

    ESP_LOGI(TAG, "Writing back and invalidating our address space");
    writeback_and_invalidate_task(task_info);

    ESP_LOGW(TAG, "Start ELF file entrypoint at %p", elf->entry);
    esp_elf_request(elf, 0, task_info->argc, task_info->argv);

    ESP_LOGI(TAG, "Successfully exited from ELF file");
    return;

out:
    // All allocations will be cleaned up Hades
}

// This runs inside the user task
static void elf_task_path(task_info_t *task_info) {
    int fd = why_open(task_info->file_path, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGW("elf_task_path", "Unable to open file");
        return;
    }

    off_t size = why_lseek(fd, 0, SEEK_END);
    // 112 bytes is currently the smallest known ELF :)
    if (size == -1 || size < 112) {
        ESP_LOGW("elf_task_path", "Unable to read file");
        return;
    }

    why_lseek(fd, 0, SEEK_SET);

    task_info->buffer = dlmalloc(size);
    if (!task_info->buffer) {
        ESP_LOGW("elf_task_path", "Unable to allocate memory");
        return;
    }

    ssize_t r = why_read(fd, task_info->buffer, size);
    if (r != size) {
        ESP_LOGW("elf_task_path", "Unable to read file contents");
        return;
    }

    why_close(fd);
    elf_task(task_info);
}

// This is the function that runs inside the Task
static void generic_task(void *ti) {
    // Final setup to be done inside of the task context before we launch our entrypoint
    task_info_t *task_info = ti;
    // ESP_LOGI(TAG, "Setting watchpoint on %p core %i", &task_info->pad, esp_cpu_get_core_id());
    // esp_cpu_set_watchpoint(0, &task_info->pad, 4, ESP_CPU_WATCHPOINT_STORE);

    // YOLO
    task_info->task_entry(task_info);

    vTaskDelete(NULL);
}

void task_record_resource_alloc(task_resource_type_t type, void *ptr) {
    task_info_t *task_info = get_task_info();

    if (task_info && task_info->pid) {
        khash_insert_unique_ptr(
            restable,
            task_info->resources[type],
            ptr,
            type,
            "Attempted allocate already allocated resource"
        );
    }
}

void task_record_resource_free(task_resource_type_t type, void *ptr) {
    task_info_t *task_info = get_task_info();

    if (task_info && task_info->pid) {
        khash_del_ptr(restable, task_info->resources[type], ptr, "Attempted to free already freed resource");
    }
}

pid_t run_task_path(char const *path, uint16_t stack_size, task_type_t type, int argc, char *argv[]) {
    if (!(type == TASK_TYPE_ELF || type == TASK_TYPE_ELF_PATH)) {
        ESP_LOGE(TAG, "Can only run ELF non-rom files");
        return -1;
    }

    type = TASK_TYPE_ELF_PATH;

    int fd = why_open(path, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGW(TAG, "Could not open %s", path);
        return -1;
    }

    why_close(fd);

    return run_task((void const *)path, stack_size, type, argc, argv);
}

pid_t run_task(void const *buffer, uint16_t stack_size, task_type_t type, int argc, char *argv[]) {
    task_info_t *parent_task_info = get_task_info();
    pid_t        pid              = pid_allocate();
    if (pid <= 0) {
        ESP_LOGW(TAG, "Cannot allocate PID for new task");
        return -1;
    }

    stack_size = stack_size < MIN_STACK_SIZE ? MIN_STACK_SIZE : stack_size;

    task_info_t *task_info = task_info_init();
    if (!task_info) {
        goto error;
    }

    task_info->heap_start = (uintptr_t)VADDR_TASK_START;
    task_info->heap_end   = task_info->heap_start;
    // ESP_LOGI(TAG, "Setting watchpoint on %p core %i", &task_info->pad, esp_cpu_get_core_id());
    // esp_cpu_set_watchpoint(0, &task_info->pad, 4, ESP_CPU_WATCHPOINT_STORE);

    task_info->type       = type;
    task_info->pid        = pid;
    task_info->parent     = parent_task_info->pid;
    task_info->buffer     = buffer;
    task_info->file_path  = NULL;
    task_info->argc       = argc;
    task_info->stack_size = stack_size;

    // Pack up argv in a nice compact list
    size_t argv_size = argc * sizeof(char *);
    for (int i = 0; i < argc; ++i) {
        argv_size += strlen(argv[i]) + 1;
    }

    task_info->argv = malloc(argv_size);
    if (!task_info->argv) {
        ESP_LOGE(TAG, "Out of memory trying to allocate argv buffer");
        goto error;
    }

    // In case someone tries something clever
    task_info->argv_back = task_info->argv;
    task_info->argv_size = argv_size;

    size_t offset = argc * sizeof(char *);
    for (int i = 0; i < argc; ++i) {
        char *arg_address  = ((void *)task_info->argv) + offset;
        task_info->argv[i] = arg_address;
        strcpy(arg_address, argv[i]);
        offset += strlen(argv[i]) + 1;
    }

    switch (type) {
        case TASK_TYPE_ELF: task_info->task_entry = elf_task; break;
        case TASK_TYPE_ELF_PATH:
            task_info->file_path  = strdup((char const *)buffer);
            task_info->buffer     = NULL;
            task_info->task_entry = elf_task_path;
            break;
        default: ESP_LOGE(TAG, "Unknown task type %i", type); goto error;
    }

    xQueueSend(zeus_queue, &task_info, portMAX_DELAY);
    return pid;
error:
    task_info_delete(task_info);
    pid_free(pid);
    return -1;
}

void IRAM_ATTR task_switched_in_hook(TaskHandle_t volatile *handle) {
    task_info_t *task_info = get_task_info();
    if (task_info && task_info->pid) {
        // ESP_DRAM_LOGW(DRAM_STR("task_switched_hook"), "Switching to task %u, heap_start %p, heap_end %p",
        // task_info->pid, (void*)task_info->heap_start, (void*)task_info->heap_end);
        remap_task(task_info);
    }
}

void IRAM_ATTR task_switched_out_hook(TaskHandle_t volatile *handle) {
    task_info_t *task_info = get_task_info();
    if (task_info && task_info->pid) {
        // ESP_DRAM_LOGW(DRAM_STR("task_switched_hook"), "Switching to task %u, heap_start %p, heap_end %p",
        // task_info->pid, (void*)task_info->heap_start, (void*)task_info->heap_end);
        unmap_task(task_info);
    }
}

uint32_t get_num_tasks() {
    return num_tasks;
}

static void IRAM_ATTR hades(void *ignored) {
    pid_t dead_pid;

    while (1) {
        // Block until a task dies
        if (xQueueReceive(hades_queue, &dead_pid, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("HADES", "Stripping PID %d of its worldy possessions", dead_pid);

            task_info_t *task_info = process_table[dead_pid];
            if (task_info) {
                switch (task_info->type) {
                    case TASK_TYPE_ELF_PATH: // Fallthrough
                    case TASK_TYPE_ELF: free(task_info->data); break;
                    default: ESP_LOGE(TAG, "Unknown task type %i", task_info->type);
                }

                process_table_remove_task(task_info);
                if (task_info->heap_size) {
                    pages_deallocate(task_info->allocations);
                }
                task_info_delete(task_info);

                // Don't free our PID until the last moment
                pid_free(dead_pid);
                ESP_LOGI("HADES", "Task %d escorted to my realm", dead_pid);
                --num_tasks;
            } else {
                ESP_LOGE("HADES", "Task %d has no task info?", dead_pid);
            }
        }
    }
}

static void IRAM_ATTR zeus(void *ignored) {
    task_info_t *task_info;
#if (NUM_PIDS > 999)
#error "If you hit this assertion change the allocation for the task name to match the new size"
#endif
    // "Task " + "255" + \0
    char task_name[5 + 3 + 1];

    while (1) {
        // Block until a new tasks wants to be born
        if (xQueueReceive(zeus_queue, &task_info, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("ZEUS", "Breathing life into PID %d", task_info->pid);
            TaskHandle_t new_task;
            BaseType_t   res = xTaskCreatePinnedToCore(
                generic_task,
                task_name,
                task_info->stack_size,
                (void *)task_info,
                5,
                &new_task,
                1
            );
            if (res == pdPASS) {
                // Since Zeus is the highest priority task on the core the task should never be able to run
                task_info->handle = new_task;
                process_table_add_task(task_info);
                vTaskSetThreadLocalStoragePointer(new_task, 0, task_info);
                ESP_LOGV("ZEUS", "PID %d sprung forth fully formed from my forehead", task_info->pid);
                ++num_tasks;
            } else {
                ESP_LOGE("ZEUS", "PID %d could not be started, too good for this world", task_info->pid);
                pid_free(task_info->pid);
                task_info_delete(task_info);
            }
        }
    }
}

void kernel_task_base(void *pvParameters) {
    kernel_task_param_t *params = pvParameters;
    vTaskSetThreadLocalStoragePointer(NULL, 0, &kernel_task);

    TaskFunction_t entry           = params->entry;
    void          *task_parameters = params->pvParameters;

    free(params);

    entry(task_parameters);
}

BaseType_t create_kernel_task(
    TaskFunction_t      pvTaskCode,
    char const *const   pcName,
    uint32_t const      usStackDepth,
    void *const         pvParameters,
    UBaseType_t         uxPriority,
    TaskHandle_t *const pvCreatedTask,
    BaseType_t const    xCoreID
) {
    kernel_task_param_t *params = calloc(1, sizeof(kernel_task_param_t));
    params->entry               = pvTaskCode;
    params->pvParameters        = pvParameters;

    BaseType_t ret =
        xTaskCreatePinnedToCore(kernel_task_base, pcName, usStackDepth, params, uxPriority, pvCreatedTask, xCoreID);
    return ret;
}

void task_init() {
    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Initializing");

    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Creating PID table");
    pid_table_lock = xSemaphoreCreateMutex();

    for (int i = 0; i < NUM_PIDS; ++i) {
        pid_table[i] = i;
    }

    // Reserve pid 0
    pid_allocate();

    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Creating Process table");
    process_table_lock = xSemaphoreCreateMutex();
    memset(process_table, 0, sizeof(process_table));

    ESP_DRAM_LOGI(
        DRAM_STR("task_init"),
        "Registering Kernel task structure at %p - %p",
        &kernel_task,
        ((uintptr_t)&kernel_task) + sizeof(task_info_t)
    );
    vTaskSetThreadLocalStoragePointer(NULL, 0, &kernel_task);
    process_table_add_task(&kernel_task);

    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Starting Hades process");
    hades_queue = xQueueCreate(16, sizeof(pid_t));
    // Hades has higher priority than Zeus. This prevents dead tasks from piling up
    // while Zeus tries to spawn new ones
    create_kernel_task(hades, "Hades", 3072, NULL, 11, &hades_handle, 1);

    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Starting Zeus process");
    zeus_queue = xQueueCreate(16, sizeof(task_info_t *));
    create_kernel_task(zeus, "Zeus", 2048, NULL, 10, &zeus_handle, 1);
}

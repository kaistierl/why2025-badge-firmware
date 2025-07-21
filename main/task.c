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
#include "esp_elf.h"
#include "esp_log.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "hal/cache_types.h"
#include "hal/mmu_hal.h"
#include "hal/mmu_ll.h"
#include "hal/mmu_types.h"
#include "hash_helper.h"
#include "khash.h"
#include "memory.h"

#include "esp_private/esp_cpu_internal.h"

#include <stdatomic.h>

#include <iconv.h>
#include <regex.h>
#include <string.h>

extern void spi_flash_enable_interrupts_caches_and_other_cpu(void);
extern void spi_flash_disable_interrupts_caches_and_other_cpu(void);
extern void remap_task(task_info_t *task_info);
extern void unmap_task(task_info_t *task_info);
extern void __real_xt_unhandled_exception(void *frame);

static char const *TAG = "task";

IRAM_ATTR static task_info_psram_t kernel_task_psram;
IRAM_ATTR static task_info_t       kernel_task = {
          .heap_start = KERNEL_HEAP_START,
          .heap_end   = KERNEL_HEAP_START,
          .psram      = &kernel_task_psram,
};

// Tracks free PIDs. Note that free PIDs are marked as 1, not 0!
static uint32_t          pid_free_bitmap[NUM_PIDS / 32];
static SemaphoreHandle_t pid_free_bitmap_lock = NULL;
static pid_t             next_pid;
static uint32_t          num_tasks = 0;

// Process table, each process gets a PID, which we track here.
static task_info_t      *process_table[NUM_PIDS];
static SemaphoreHandle_t process_table_lock = NULL;

static TaskHandle_t  hades_handle;
static QueueHandle_t hades_queue;

static pid_t pid_allocate() {
    if (xSemaphoreTake(pid_free_bitmap_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    // First see if our next pid is free
    pid_t    pid      = next_pid;
    uint32_t word_idx = pid / 32;
    uint32_t bit_idx  = pid % 32;

    if (bitcheck(pid_free_bitmap[word_idx], bit_idx)) {
        // next_pid is free. claim it.
        bitclear(pid_free_bitmap[word_idx], bit_idx);
        goto out;
    }

    // No, we will need to scan our bitmap
    for (int i = 0; i < (sizeof(pid_free_bitmap) / 4); ++i) {
        // We want to loop through all of our bitmap, but we want to start at
        // where we expect the next free pid to be
        int check_word_idx = (i + word_idx) % (sizeof(pid_free_bitmap) / 4);

        if (!pid_free_bitmap[check_word_idx]) {
            // all zeros no need to check
            continue;
        }

        for (int k = 0; k < 32; ++k) {
            if (bitcheck(pid_free_bitmap[check_word_idx], k)) {
                bitclear(pid_free_bitmap[check_word_idx], k);
                pid = check_word_idx * 32 + k;
                goto out;
            }
        }
    }

    // No free pids
    pid = -1;
    goto error;

out:
    next_pid = pid + 1;

    if (next_pid > MAX_PID) {
        // PIDs looped, start over
        next_pid = 1;
    }

error:

    xSemaphoreGive(pid_free_bitmap_lock);
    return pid;
}

task_info_t *get_taskinfo_for_pid(pid_t pid) {
    return process_table[pid];
}

static void pid_free(pid_t pid) {
    uint32_t word_idx = pid / 32;
    uint32_t bit_idx  = pid % 32;

    if (xSemaphoreTake(pid_free_bitmap_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get pid table mutex");
        abort();
    }

    bitset(pid_free_bitmap[word_idx], bit_idx);

    xSemaphoreGive(pid_free_bitmap_lock);
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
    if (xPortInIsrContext()) {
        ESP_DRAM_LOGE(DRAM_STR("TASK KILLED"), "vTaskPreDeletionHook from ISR");
        return;
    }

    task_info_t *task_info = pvTaskGetThreadLocalStoragePointer(handle, 0);
    if (task_info) {
        pid_t pid = task_info->pid;
        xQueueSend(hades_queue, &pid, portMAX_DELAY);
    }
}

static task_info_t *task_info_init() {
    task_info_t *task_info = calloc(1, sizeof(task_info_t));
    if (!task_info) {
        ESP_LOGE(TAG, "Out of memory trying to allocate task info");
        return NULL;
    }

    task_info->psram = heap_caps_calloc(1, sizeof(task_info_psram_t), MALLOC_CAP_SPIRAM);
    if (!task_info->psram) {
        ESP_LOGE(TAG, "Out of memory trying to allocate task info");
        free(task_info);
        return NULL;
    }

    task_info->current_files                  = 3;
    task_info->psram->file_handles[0].is_open = true;
    task_info->psram->file_handles[0].device  = device_get("TT01");
    task_info->psram->file_handles[1].is_open = true;
    task_info->psram->file_handles[1].device  = device_get("TT01");
    task_info->psram->file_handles[2].is_open = true;
    task_info->psram->file_handles[2].device  = device_get("TT01");

    for (int i = 0; i < RES_RESOURCE_TYPE_MAX; ++i) {
        task_info->resources[i] = kh_init(restable);
    }

    return task_info;
}

static void task_info_delete(task_info_t *task_info) {
    ESP_LOGI(TAG, "Deleting task_info %pi for pid %i", task_info, task_info->pid);

    for (int i = 0; i < MAXFD; ++i) {
        // We sadly can't reuse the why_close code as it must be ran from inside the user task
        if (task_info->psram->file_handles[i].is_open) {
            ESP_LOGI(TAG, "Cleaning up open filehandle %i", i);
            if (task_info->psram->file_handles[i].device->_close) {
                task_info->psram->file_handles[i].device->_close(
                    task_info->psram->file_handles[i].device,
                    task_info->psram->file_handles[i].dev_fd
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
                    default: ESP_LOGE(TAG, "Unknown resource type %i in task_info_delete", type);
                }
            }
        }

        kh_destroy(restable, task_info->resources[i]);
    }

    free(task_info->argv_back);
    heap_caps_free(task_info->psram);
    free(task_info);
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
    bool should_retry = false;
    task_info_t *task_info = NULL;

    task_info = get_task_info();
    if (task_info && task_info->pid) {
        esp_rom_printf("*************** PID %u  MCAUSE 0x%02lx ******************\n", task_info->pid, ((RvExcFrame *)frame)->mcause);

        // Define which exceptions you want to retry
        switch (((RvExcFrame *)frame)->mcause) {
            case 1:   /* Instruction access fault */
            case 5:   /* Load access fault */
            case 7:   /* Store/AMO access fault */
            case 12:  /* Instruction page fault */
            case 13:  /* Load page fault */
            case 15:  /* Store/AMO page fault */
                should_retry = true;
                break;
            case 0:   /* Instruction address misaligned */
            case 2:   /* Illegal instruction */
            case 3:   /* Breakpoint */
            case 4:   /* Load address misaligned */
            case 6:   /* Store/AMO address misaligned */
            case 8:   /* Environment call from U-mode */
            case 9:   /* Environment call from S-mode */
            // case 10:  /* Reserved */
            case 11:  /* Environment call from M-mode */
            // case 14:  /* Reserved */
                break;
        };

        if (should_retry) {
            esp_rom_printf("*************** RETRYING AFTER PAUSE FOR PID %u ******************\n", task_info->pid);
            __builtin_riscv_pause();
            __builtin_riscv_pause();
            __builtin_riscv_pause();
            // lets give love a chance
            __asm__ volatile("mret\n\t");
        }

        esp_rom_printf("Task %u caused an unhandled exception, Cerberos will deal with it\n", task_info->pid);
        __asm__ volatile("csrw mepc, %0\n\t" // Set return address
                         "mret\n\t"
                         :
                         : "r"(cerberos)
                         : "t0", "memory");
    }
    __real_xt_unhandled_exception(frame);
}

static void IRAM_ATTR NOINLINE_ATTR hades(void *ignored) {
    pid_t dead_pid;

    while (1) {
        // Block until a task dies
        if (xQueueReceive(hades_queue, &dead_pid, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("HADES", "Stripping PID %d of its worldy possessions", dead_pid);

            task_info_t *task_info = process_table[dead_pid];
            if (task_info) {
                switch (task_info->type) {
                    case TASK_TYPE_ELF:
                    case TASK_TYPE_ELF_ROM: free(task_info->data); break;
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

    if (!task_info->buffer_in_rom) {
        free(task_info->buffer);
    }

    ESP_LOGI(TAG, "Start ELF file entrypoint");

    esp_elf_request(elf, 0, task_info->argc, task_info->argv);

    ESP_LOGI(TAG, "Successfully exited from ELF file");
    return;

out:
    // All allocations will be cleaned up Hades
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

    khash_insert_unique_ptr(
        restable,
        task_info->resources[type],
        ptr,
        type,
        "Attempted allocate already allocated resource"
    );
}

void task_record_resource_free(task_resource_type_t type, void *ptr) {
    task_info_t *task_info = get_task_info();

    khash_del_ptr(restable, task_info->resources[type], ptr, "Attempted to free already freed resource");
}

pid_t run_task(void *buffer, int stack_size, task_type_t type, int argc, char *argv[]) {
    pid_t pid = pid_allocate();
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

    task_info->type          = type;
    task_info->pid           = pid;
    task_info->buffer        = buffer;
    task_info->buffer_in_rom = false;
    task_info->argc          = argc;

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
        case TASK_TYPE_ELF_ROM:
            task_info->task_entry    = elf_task;
            task_info->buffer_in_rom = true;
            break;
        default:
            ESP_LOGE(TAG, "Unknown task type %i", type);
            goto error;
    }

#if (NUM_PIDS > 999)
#error "If you hit this assertion change the allocation for the task name to match the new size"
#endif

    // "Task " + "255" + \0
    char task_name[5 + 3 + 1];
    sprintf(task_name, "Task %i", (uint8_t)pid);

    // xTaskCreate(generic_task, task_name, stack_size, (void *)task_info, 5, NULL);
    ++num_tasks;
    TaskHandle_t new_task;
    BaseType_t res = xTaskCreatePinnedToCore(generic_task, task_name, stack_size, (void *)task_info, 5, &new_task, 1);
    if (res == pdPASS) {
        vTaskSuspend(new_task);
        task_info->handle = new_task;
        process_table_add_task(task_info);
        // vTaskSetThreadLocalStoragePointerAndDelCallback(new_task, 0, task_info, task_killed);
        vTaskSetThreadLocalStoragePointer(new_task, 0, task_info);
        vTaskResume(new_task);
        return pid;
    }

error:
    free(task_info);
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

uint32_t get_num_tasks() {
    return num_tasks;
}

void task_init() {
    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Initializing");

    ESP_DRAM_LOGI(DRAM_STR("task_init"), "Creating PID table");
    pid_free_bitmap_lock = xSemaphoreCreateMutex();
    memset(pid_free_bitmap, 0xFF, sizeof(pid_free_bitmap));

    pid_free_bitmap[0] = UINT32_MAX - 1; // PID 0 is the kernel

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
    xTaskCreatePinnedToCore(hades, "Hades", 2048, NULL, 10, &hades_handle, 0);
    vTaskSetThreadLocalStoragePointer(hades_handle, 0, &kernel_task);

    next_pid = 1;
}

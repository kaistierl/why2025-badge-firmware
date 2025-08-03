#include "esp_attr.h"
#include "task.h"

extern task_info_t kernel_task;

__attribute__((always_inline)) static inline IRAM_ATTR struct malloc_state *get_malloc_state() {
    task_info_t *task_info = get_task_info();
    if (!task_info) {
        return &kernel_task.thread->malloc_state;
    }
    return &task_info->thread->malloc_state;
}

__attribute__((always_inline)) static inline IRAM_ATTR struct malloc_params *get_malloc_params() {
    task_info_t *task_info = get_task_info();
    if (!task_info) {
        return &kernel_task.thread->malloc_params;
    }
    return &task_info->thread->malloc_params;
}

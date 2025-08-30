/* Local stub for badgevms/process.h 
 * Provides basic threading compatibility for local builds
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>

// For local builds, use pthread instead of BadgeVMS threading
typedef struct {
    pthread_t pthread;
    void (*entry)(void *);
    void *user_data;
} local_thread_t;

// Wrapper function to adapt pthread to BadgeVMS thread entry signature
static void* pthread_wrapper(void* arg) {
    local_thread_t* thread = (local_thread_t*)arg;
    thread->entry(thread->user_data);
    return NULL;
}

// BadgeVMS-compatible thread creation using pthread
static inline pid_t thread_create(void (*thread_entry)(void *user_data), void *user_data, uint16_t stack_size) {
    (void)stack_size; // Stack size not used in pthread version
    
    static int thread_counter = 1000; // Start from 1000 to avoid conflicts
    
    local_thread_t* thread = malloc(sizeof(local_thread_t));
    if (!thread) return -1;
    
    thread->entry = thread_entry;
    thread->user_data = user_data;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    if (pthread_create(&thread->pthread, &attr, pthread_wrapper, thread) != 0) {
        free(thread);
        return -1;
    }
    
    pthread_attr_destroy(&attr);
    
    return thread_counter++; // Return unique thread ID
}

// Stub for process_create (not used in sshterm)
static inline pid_t process_create(char const *path, size_t stack_size, int argc, char **argv) {
    (void)path; (void)stack_size; (void)argc; (void)argv;
    return -1; // Not implemented for local builds
}

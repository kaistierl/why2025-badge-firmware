#include "misc_funcs.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define NUM_ITERATIONS 128
#define ALLOC_SIZE     4096

int volatile should_exit = 0;

int main(int argc, char *argv[]) {
    struct timeval start, end;
    gettimeofday(&start, NULL);
    char task_id = (char)getpid();

    char *ptrs[100];

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Allocate some memory
        char *ptr = malloc(ALLOC_SIZE);
        // if (task_id == 2) {
        //     *((char*)((void*)0x4c100000)) = '0';
        //}
        if (ptr) {
            memset(ptr, task_id, ALLOC_SIZE);
            ptrs[i % 100] = ptr;

            // Free older allocations to avoid OOM
            if (i > 50 && ptrs[(i - 50) % 100]) {
                if (ptrs[(i - 50) % 100]) {
                    for (int k = 0; k < ALLOC_SIZE; ++k) {
                        char volatile t;
                        // Read all memory back
                        t = ptrs[(i - 50) % 100][k];

                        if (t != task_id) {
                            uintptr_t vaddr = (uintptr_t)(&ptrs[(i - 50) % 100][0]);
                            // uintptr_t vaddr_aligned = ((uintptr_t)vaddr) & ~(65535 - 1);
                            printf(
                                "\033[0;31mError unexpected memory contents! expected 0x%X got 0x%X (%c) in ptr number "
                                "%i, vaddr %p, paddr 0x%08lx\033[0m\n",
                                task_id,
                                t,
                                t,
                                (i - 50) % 100,
                                &ptrs[(i - 50) % 100][k],
                                vaddr_to_paddr(vaddr)
                            );

#if 0
                            for (int l = 0; l < 40; ++l) {
                                printf("0x%08X: ", vaddr + (l * 40));
                                for (int p = 0; p < 40; ++p) {
                                    printf("%02x ", ((char*)(vaddr))[(l * 40) + p]);
                                }
                                printf("\n");
                            }
#endif
                            die("Unexpected memory contents in memory bench");
                            return 1;
                        }
                        ptrs[(i - 50) % 100][k] = '$';
                    }
                    free(ptrs[(i - 50) % 100]);
                    ptrs[(i - 50) % 100] = NULL;
                }
            }
        }

        // Force context switch every few iterations
        if (i % 10 == 0) {
            usleep(1);
        }
    }

    // Clean up remaining allocations
    for (int i = 0; i < 100; i++) {
        if (ptrs[i])
            free(ptrs[i]);
    }

    gettimeofday(&end, NULL);

    long microseconds = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);

    printf("Task %d completed in %ld microseconds\n", task_id, microseconds);
    printf("Task %d: %ld microseconds per iteration\n", task_id, microseconds / NUM_ITERATIONS);
}

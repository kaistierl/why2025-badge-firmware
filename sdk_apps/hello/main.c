#include "badgevms/process.h"

#include <stdatomic.h> // for atomic variables
#include <stdbool.h>   // for bool type
#include <stdio.h>
#include <stdlib.h> // for atoi

#include <time.h>
#include <unistd.h> // for usleep

// The current version of this application.
int const CURRENT_VERSION = 21;

bool should_stop        = false;
int         process_start_time = -1;

/**
 * @brief The main entry point of the application.
 *
 * It starts the version checker thread and then enters a loop that prints a
 * message every second until the 'should_stop' flag is set to true by the
 * other thread.
 */
time_t unix_timestamp;

void version_logger_thread(void *data) {
    while (true) {
        printf("__BADGEHUB VLOG: APP V[%d] started at[%ld]\n", process_start_time, (long)unix_timestamp);
        usleep(5 * 1000 * 1000);
    }
}

int main(int argc, char *argv[]) {
    unix_timestamp = time(NULL);
    printf("The current Unix timestamp is: %ld\n", (long)unix_timestamp);
    thread_create(version_logger_thread, NULL, 4096);

    // Loop until the version_checker_thread signals us to stop.
    while (!should_stop) {
        // TODO badgehub loop?
        int one_second_in_usec = 1000 * 1000;
        // Sleep for one second
        usleep(one_second_in_usec);
        printf("BADGEHUB MAIN: APP V[%d] started at[%ld]\n", process_start_time, (long)unix_timestamp);
    }

    printf("Exiting due to version update.\n");
    return 0;
}
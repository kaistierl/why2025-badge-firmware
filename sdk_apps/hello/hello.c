#include "badgevms/process.h"
#include <time.h>

#include <stdatomic.h> // for atomic variables
#include <stdbool.h>   // for bool type
#include <stdio.h>
#include <stdlib.h> // for atoi

#include <time.h>
#include <unistd.h> // for usleep

// The current version of this application.
int const CURRENT_VERSION = 20;

atomic_bool should_stop     = false;

int main(int argc, char *argv[]) {

    // Loop until the version_checker_thread signals us to stop.
    while (!atomic_load(&should_stop)) {
        printf("Hello BadgeVMS world! V%d!\n", CURRENT_VERSION);
        int one_second_in_usec = 1000 * 1000;
        // Sleep for one second
        usleep(one_second_in_usec);
    }

    printf("Exiting due to version update.\n");
    return 0;
}
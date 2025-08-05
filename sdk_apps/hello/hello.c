#include "badgevms/process.h"

#include <stdatomic.h> // for atomic variables
#include <stdbool.h>   // for bool type
#include <stdio.h>
#include <stdlib.h> // for atoi

#include <unistd.h> // for usleep

// The current version of this application.
int const CURRENT_VERSION = 16;

// A thread-safe boolean flag to signal the main loop to exit.
// We use atomic_bool to prevent race conditions between the threads.
atomic_bool should_stop     = false;
int         startup_version = -1;

int get_file_version() {

    FILE *f = fopen("FLASH0:hello_version.txt", "r");

    if (f) {
        char buffer[16] = {0}; // Buffer to hold file content
        fread(buffer, 1, sizeof(buffer) - 1, f);
        fclose(f);

        return atoi(buffer); // Convert text to integer
    }
    return -1;
}
/**
 * @brief This thread function runs in a loop, checking for a newer version.
 *
 * The thread sleeps for one second, then opens and reads the version number
 * from "FLASH0:hello_version.txt". If the version in the file is higher than
 * CURRENT_VERSION, it sets the global 'should_stop' flag to true.
 *
 * @param data Unused user data pointer.
 */
void version_checker_thread(void *data) {
    // This loop runs until a new version is found.
    while (true) {
        // Sleep for 1 second
        usleep(1000 * 1000);

        // BadgeVMS paths are of the format DEVICE:[optional.dirs]filename.ext

        int new_version = get_file_version();
        // Check if the version from the file is greater
        if (new_version > startup_version) {
            printf("New version %d found, preparing to shut down.\n", new_version);
            atomic_store(&should_stop, true); // Set the flag to stop the main loop
            break;                            // Exit the checker thread's loop
        }
    }
}

/**
 * @brief The main entry point of the application.
 *
 * It starts the version checker thread and then enters a loop that prints a
 * message every second until the 'should_stop' flag is set to true by the
 * other thread.
 */
int main(int argc, char *argv[]) {
    startup_version = get_file_version();
    // Create and start the version checker thread.
    // Based on the example, we'll use a stack size of 4096.
    // thread_create(version_checker_thread, NULL, 4096);

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
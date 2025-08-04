#include <stdio.h>
#include <unistd.h> // for usleep

int main(int argc, char *argv[]) {
    while (1) {
        printf("Hello BadgeVMS world! V0!\n");
        int one_second_in_usec = 1000 * 1000;
        // Sleep
        usleep(one_second_in_usec);
    }
}

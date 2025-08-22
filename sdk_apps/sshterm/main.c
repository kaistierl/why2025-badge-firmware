#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "components/app_controller/app_controller.h"

static app_controller_t* g_app_controller = NULL;

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    printf("Starting SSH Terminal application...\n");
    
    // Initialize application
    if (!app_controller_init(&g_app_controller)) {
        fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }
    
    // Run application
    printf("Application initialized, starting main loop...\n");
    bool success = app_controller_run(g_app_controller);
    
    // Cleanup
    printf("Application finished, cleaning up...\n");
    app_controller_shutdown(g_app_controller);
    
    return success ? 0 : 1;
}

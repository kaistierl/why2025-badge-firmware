#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <stdbool.h>
#include "../term/term.h"

// Handle keyboard input events
void handle_key_event(const SDL_KeyboardEvent* key, bool* running);

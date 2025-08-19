#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <stdbool.h>
#include "../term/term.h"

// Handle keyboard input events
// Returns true if the key event was fully handled and no text input should be processed
bool handle_key_event(const SDL_KeyboardEvent* key, bool* running);

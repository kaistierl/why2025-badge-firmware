#include "keyboard.h"

// VT100/xterm escape sequences for special keys
static const char* const ARROW_UP    = "\x1b[A";
static const char* const ARROW_DOWN  = "\x1b[B";
static const char* const ARROW_RIGHT = "\x1b[C";
static const char* const ARROW_LEFT  = "\x1b[D";
static const char* const HOME_KEY    = "\x1b[H";
static const char* const END_KEY     = "\x1b[F";
static const char* const INSERT_KEY  = "\x1b[2~";
static const char* const PAGE_UP     = "\x1b[5~";
static const char* const PAGE_DOWN   = "\x1b[6~";

/**
 * Generic modifier key handling structure
 */
typedef struct {
    bool ctrl;
    bool alt;
    bool shift;
} modifier_state_t;

/**
 * Extract modifier state from SDL modifiers
 */
static modifier_state_t get_modifier_state(uint16_t mods) {
    modifier_state_t state = {0};
    state.ctrl = (mods & SDL_KMOD_CTRL) != 0;
    state.alt = (mods & SDL_KMOD_ALT) != 0;
    state.shift = (mods & SDL_KMOD_SHIFT) != 0;
    return state;
}

/**
 * Handle Alt + letter combinations (sends ESC + letter)
 */
static bool handle_alt_letter(SDL_Keycode sym, const modifier_state_t* modifiers) {
    if (!modifiers->alt || sym < SDLK_A || sym > SDLK_Z) {
        return false;
    }
    
    // Alt + letter sends ESC followed by the letter
    char letter = (char)(sym - SDLK_A + 'a'); // Convert to lowercase
    if (modifiers->shift) {
        letter = (char)(sym - SDLK_A + 'A'); // Keep uppercase if shift is also pressed
    }
    
    // Send ESC + letter sequence
    char sequence[3] = {'\x1b', letter, '\0'};
    term_key_input(0, 0, sequence);
    return true;
}

/**
 * Handle Ctrl + letter combinations
 */
static bool handle_ctrl_letter(SDL_Keycode sym, const modifier_state_t* modifiers) {
    if (!modifiers->ctrl || sym < SDLK_A || sym > SDLK_Z) {
        return false;
    }
    
    // Pass the letter with Ctrl modifier to terminal for proper processing
    term_key_input(sym, modifiers->ctrl ? SDL_KMOD_CTRL : 0, NULL);
    return true;
}

/**
 * Handle special keys with conditional modifier passing
 */
static void handle_special_key(int keysym, const char* sequence, const modifier_state_t* modifiers) {
    if (modifiers->ctrl || modifiers->alt || modifiers->shift) {
        // Reconstruct modifier flags for term_key_input
        uint16_t mods = 0;
        if (modifiers->ctrl) mods |= SDL_KMOD_CTRL;
        if (modifiers->alt) mods |= SDL_KMOD_ALT;
        if (modifiers->shift) mods |= SDL_KMOD_SHIFT;
        
        if (sequence) {
            term_key_input(0, mods, sequence);
        } else {
            term_key_input(keysym, mods, NULL);
        }
    } else {
        if (sequence) {
            term_key_input(0, 0, sequence);
        } else {
            term_key_input(keysym, 0, NULL);
        }
    }
}

/**
 * Handle SDL keyboard events and convert them to terminal input
 * 
 * @param key The SDL keyboard event
 * @param running Pointer to the main loop running flag (for Ctrl+Q exit)
 */
void handle_key_event(const SDL_KeyboardEvent* key, bool* running) {
    // Input validation
    if (!key || !running) {
        return;
    }
    
    SDL_Keycode sym = key->key;
    uint16_t mods = key->mod;
    modifier_state_t modifiers = get_modifier_state(mods);

    // Handle Ctrl+Q to quit the application  
    if (modifiers.ctrl && sym == SDLK_Q) {
        *running = false;
        return;
    }

    // Handle Alt + letter combinations first (Alt has precedence over Ctrl for letters)
    if (handle_alt_letter(sym, &modifiers)) {
        return;
    }

    // Handle Ctrl + letter combinations
    if (handle_ctrl_letter(sym, &modifiers)) {
        return;
    }

    // Handle special keys with modifiers passed through
    switch (sym) {
        case SDLK_ESCAPE:
            handle_special_key('\x1b', NULL, &modifiers);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            handle_special_key('\r', NULL, &modifiers);
            break;
            
        case SDLK_BACKSPACE:
            handle_special_key('\b', NULL, &modifiers);
            break;
            
        case SDLK_DELETE:
            handle_special_key(127, NULL, &modifiers);
            break;

        case SDLK_TAB:
            handle_special_key('\t', NULL, &modifiers);
            break;
            
        // Arrow keys with modifier support (e.g., Alt+arrow for word navigation)
        case SDLK_UP:
            handle_special_key(0, ARROW_UP, &modifiers);
            break;
            
        case SDLK_DOWN:
            handle_special_key(0, ARROW_DOWN, &modifiers);
            break;
            
        case SDLK_LEFT:
            handle_special_key(0, ARROW_LEFT, &modifiers);
            break;
            
        case SDLK_RIGHT:
            handle_special_key(0, ARROW_RIGHT, &modifiers);
            break;
            
        case SDLK_HOME:
            handle_special_key(0, HOME_KEY, &modifiers);
            break;
            
        case SDLK_END:
            handle_special_key(0, END_KEY, &modifiers);
            break;
            
        case SDLK_INSERT:
            handle_special_key(0, INSERT_KEY, &modifiers);
            break;
            
        case SDLK_PAGEUP:
            handle_special_key(0, PAGE_UP, &modifiers);
            break;
            
        case SDLK_PAGEDOWN:
            handle_special_key(0, PAGE_DOWN, &modifiers);
            break;
            
        default:
            // Let SDL_EVENT_TEXT_INPUT handle regular characters
            break;
    }
}

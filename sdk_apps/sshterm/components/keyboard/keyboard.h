/**
 * @file keyboard.h
 * @brief Keyboard input handling interface
 * 
 * This file provides the public interface for processing keyboard events
 * and converting them to appropriate terminal input sequences.
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <stdbool.h>
#include "../term/term.h"

/**
 * @brief Handle keyboard input events
 * 
 * Processes SDL keyboard events and converts them to appropriate terminal
 * input sequences. Handles special keys, modifiers, and control sequences.
 * 
 * @param key Pointer to SDL keyboard event structure
 * @param running Pointer to application running flag (may be modified for quit)
 * @return true if the key event was fully handled and no further text input processing should occur
 */
bool handle_key_event(const SDL_KeyboardEvent* key, bool* running);

#endif // KEYBOARD_H

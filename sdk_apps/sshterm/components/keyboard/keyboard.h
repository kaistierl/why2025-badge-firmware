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
 * @brief Result of keyboard event processing
 */
typedef enum {
    KEYBOARD_HANDLED,          /**< Event was fully handled, suppress any follow-up text input */
    KEYBOARD_NOT_HANDLED,      /**< Event was not handled, continue with normal processing */
    KEYBOARD_QUIT_REQUESTED    /**< Quit was requested (Ctrl+Q) */
} keyboard_result_t;

/**
 * @brief Process keyboard events with enhanced modifier handling
 * 
 * This function provides better coordination between KEY_DOWN and TEXT_INPUT events
 * to properly handle modifier key combinations on different platforms.
 * 
 * @param key Pointer to SDL keyboard event structure
 * @return Result indicating how the event was processed
 */
keyboard_result_t keyboard_process_key_event(const SDL_KeyboardEvent* key);

/**
 * @brief Process text input events with modifier awareness
 * 
 * Processes text input events, but only if they weren't suppressed by
 * a previous key event with modifiers.
 * 
 * @param text Pointer to SDL text input event structure  
 * @return true if the text was processed, false if it was suppressed
 */
bool keyboard_process_text_input(const SDL_TextInputEvent* text);

#endif // KEYBOARD_H

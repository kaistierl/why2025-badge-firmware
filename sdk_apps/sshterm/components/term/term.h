/**
 * @file term.h
 * @brief Terminal emulation interface
 * 
 * This file provides the public interface for terminal emulation functionality
 * using libvterm. It handles VT100/xterm sequence processing and provides
 * a callback mechanism for terminal output.
 */

#ifndef TERM_H
#define TERM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Terminal output callback function type
 * 
 * Called when the terminal needs to send data back to the SSH connection.
 * 
 * @param data Pointer to data buffer to send
 * @param len Length of data in bytes
 * @param user User data pointer passed to term_init()
 */
typedef void (*term_write_cb)(const uint8_t* data, size_t len, void* user);

/**
 * @brief Initialize the terminal emulator
 * 
 * Sets up the terminal emulator with specified dimensions and output callback.
 * 
 * @param cols Number of terminal columns (width in characters)
 * @param rows Number of terminal rows (height in characters)
 * @param write_cb Callback function for terminal output
 * @param user User data pointer passed to callback
 * @return true on successful initialization, false on failure
 */
bool term_init(int cols, int rows, term_write_cb write_cb, void* user);

/**
 * @brief Shutdown the terminal emulator
 * 
 * Cleans up all terminal emulator resources and state.
 */
void term_shutdown(void);

/**
 * @brief Process input bytes from SSH connection
 * 
 * Processes bytes received from SSH connection through the terminal emulator.
 * This may result in screen updates and/or calls to the write callback.
 * 
 * @param data Pointer to input data buffer
 * @param len Length of input data in bytes
 */
void term_input_bytes(const uint8_t* data, size_t len);

/**
 * @brief Process input string from SSH connection
 * 
 * Convenience function for string literals. Automatically computes length.
 * 
 * @param str Null-terminated string to process
 */
void term_input_string(const char* str);

/**
 * @brief Process keyboard input
 * 
 * Converts SDL keyboard events to appropriate terminal input sequences.
 * 
 * @param keysym SDL key code (SDL_Keycode)
 * @param mods Modifier key bitmask (SDL modifier flags)
 * @param text_utf8 UTF-8 text input (may be NULL for non-text keys)
 */
void term_key_input(int keysym, uint16_t mods, const char* text_utf8);

/**
 * @brief Resize the terminal
 * 
 * Changes the terminal dimensions. Currently not used in MVP but kept
 * for future functionality.
 * 
 * @param cols New number of columns
 * @param rows New number of rows
 */
void term_resize(int cols, int rows);

#endif // TERM_H

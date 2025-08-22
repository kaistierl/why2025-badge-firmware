/**
 * @file renderer.h
 * @brief Terminal grid renderer interface
 * 
 * This file provides the public interface for rendering terminal content
 * to the screen using SDL3. It handles a fixed-size character grid with
 * optimized dirty-flag rendering.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

/**
 * @brief Fixed grid dimensions for terminal rendering
 * 
 * These constants define the terminal grid layout:
 * - Leggie 9x18 font on 720x720 display
 * - 9px top padding for visual alignment
 */
enum {
    RENDER_CELL_W = 9,    /**< Width of each character cell in pixels */
    RENDER_CELL_H = 18,   /**< Height of each character cell in pixels */
    RENDER_COLS   = 80,   /**< Number of terminal columns */
    RENDER_ROWS   = 39    /**< Number of terminal rows */
};

/**
 * @brief RGB color representation
 * 
 * Simple RGB color structure using 24-bit color values.
 * Alpha channel is not used in the current implementation.
 */
typedef struct {
    uint32_t rgb;         /**< RGB color value (0xRRGGBB format) */
} render_color_t;

/**
 * @brief Initialize the renderer
 * 
 * Sets up the renderer with existing SDL window and renderer contexts.
 * Must be called before any other renderer functions.
 * 
 * @param window SDL window for rendering
 * @param renderer SDL renderer context
 * @return true on successful initialization, false on failure
 */
bool renderer_init(SDL_Window* window, SDL_Renderer* renderer);

/**
 * @brief Shutdown the renderer
 * 
 * Cleans up all renderer resources and state.
 */
void renderer_shutdown(void);

/**
 * @brief Set content of a single terminal cell
 * 
 * Updates the character and colors for a specific grid position.
 * ASCII characters 32-126 are rendered; others display as space.
 * 
 * @param x Column position (0 to RENDER_COLS-1)
 * @param y Row position (0 to RENDER_ROWS-1)
 * @param codepoint Unicode codepoint to display
 * @param fg Foreground (text) color
 * @param bg Background color
 */
void renderer_set_cell(int x, int y, uint32_t codepoint,
                       render_color_t fg, render_color_t bg);

/**
 * @brief Scroll a region of the terminal up
 * 
 * Moves content in the specified row range up by the given number of lines.
 * Used for implementing terminal scrolling behavior.
 * 
 * @param top Top row of scroll region (inclusive)
 * @param bottom Bottom row of scroll region (inclusive)
 * @param lines Number of lines to scroll up
 */
void renderer_scroll_up(int top, int bottom, int lines);

/**
 * @brief Set cursor position and visibility
 * 
 * Controls the terminal cursor display including position and
 * whether it should be visible.
 * 
 * @param x Cursor column position
 * @param y Cursor row position
 * @param visible Whether cursor should be displayed
 */
void renderer_set_cursor(int x, int y, bool visible);

/**
 * @brief Present rendered content if dirty
 * 
 * Performs actual rendering to screen only if content has changed
 * since last call. Uses dirty flag optimization for performance.
 * 
 * @param now_ms Current timestamp in milliseconds (from SDL_GetTicks())
 */
void renderer_present_if_dirty(uint32_t now_ms);

#endif // RENDERER_H

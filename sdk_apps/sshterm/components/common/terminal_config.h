/**
 * @file terminal_config.h
 * @brief Central terminal configuration constants
 * 
 * This file defines the core terminal dimensions and constants used
 * throughout the SSH terminal application. All components should
 * reference these centralized values.
 */

#ifndef TERMINAL_CONFIG_H
#define TERMINAL_CONFIG_H

// === TERMINAL DIMENSIONS ===

/**
 * Terminal grid dimensions - these define the character-based
 * terminal size for the entire application
 */
#define TERMINAL_COLS   80    /**< Number of terminal columns */
#define TERMINAL_ROWS   39    /**< Number of terminal rows */

#endif // TERMINAL_CONFIG_H

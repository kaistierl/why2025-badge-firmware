/**
 * @file test_mode.h
 * @brief Terminal test mode interface
 * 
 * This file provides the public interface for the terminal test mode,
 * which allows testing terminal features like colors, character rendering,
 * and input handling without requiring an SSH connection.
 */

#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize test mode
 * 
 * Sets up and displays the terminal test interface including color test
 * patterns and usage instructions.
 */
void test_mode_init(void);

/**
 * @brief Handle input in test mode
 * 
 * Processes user input in test mode, providing echo functionality
 * with special character handling for testing terminal features.
 * 
 * @param data Input data buffer
 * @param len Length of input data in bytes
 */
void test_mode_handle_input(const uint8_t* data, size_t len);

#endif // TEST_MODE_H

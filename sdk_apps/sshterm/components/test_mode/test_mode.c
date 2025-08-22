#include "test_mode.h"
#include "../term/term.h"
#include <string.h>
#include <stdio.h>

void test_mode_init(void) {
    // Clear screen and move cursor to home
    term_input_string("\x1b[2J\x1b[H");

    // Terminal title and info
    term_input_string("\x1b[1;36m");  // Bold cyan
    term_input_string("=== SSHTerm Test Suite ===\r\n");
    term_input_string("\x1b[0m");  // Reset
    
    term_input_string("libvterm-0.3.3 | 80x39 grid | Leggie 9x18 font\r\n\r\n");
    
    // 8-color ANSI test
    term_input_string("\x1b[1mANSI Colors (8-color palette):\x1b[0m\r\n");
    
    // Standard colors
    const char* colors[] = {
        "\x1b[30m Black ", "\x1b[31m Red ", "\x1b[32m Green ", "\x1b[33m Yellow ",
        "\x1b[34m Blue ", "\x1b[35m Magenta ", "\x1b[36m Cyan ", "\x1b[37m White "
    };
    
    for (int i = 0; i < 8; i++) {
        term_input_string(colors[i]);
    }
    term_input_string("\x1b[0m\r\n");  // Reset + newline
    
    // Bold colors test
    term_input_string("\x1b[1mBold Colors (brightness enhanced):\x1b[0m\r\n");
    for (int i = 0; i < 8; i++) {
        term_input_string("\x1b[1m");  // Bold
        term_input_string(colors[i]);
        term_input_string("\x1b[0m");  // Reset
    }
    term_input_string("\r\n");
    
    // Background colors test
    term_input_string("\x1b[1mBackground Colors:\x1b[0m\r\n");
    const char* bg_colors[] = {
        "\x1b[40;37m Black ", "\x1b[41;37m Red ", "\x1b[42;30m Green ", "\x1b[43;30m Yellow ",
        "\x1b[44;37m Blue ", "\x1b[45;37m Magenta ", "\x1b[46;30m Cyan ", "\x1b[47;30m White "
    };
    
    for (int i = 0; i < 8; i++) {
        term_input_string(bg_colors[i]);
    }
    term_input_string("\x1b[0m\r\n");  // Reset + newline
    
    // Text attributes test
    term_input_string("\r\n\x1b[1mText Attributes:\x1b[0m\r\n");
    term_input_string("Normal text | ");
    term_input_string("\x1b[1mBold text\x1b[0m | ");
    term_input_string("\x1b[4mUnderlined\x1b[0m | ");
    term_input_string("\x1b[7mReversed\x1b[0m\r\n");
    
    // RGB color test (if supported)
    term_input_string("\r\n\x1b[1mRGB Colors (24-bit):\x1b[0m\r\n");
    term_input_string("\x1b[38;2;255;100;50mOrange\x1b[0m ");
    term_input_string("\x1b[38;2;100;255;100mLime\x1b[0m ");
    term_input_string("\x1b[38;2;100;100;255mSky\x1b[0m ");
    term_input_string("\x1b[38;2;255;50;255mPink\x1b[0m\r\n");
    
    // Character set test
    term_input_string("\x1b[1mASCII Test (32-126):\x1b[0m\r\n");
    term_input_string("!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnop\r\n");
    term_input_string("qrstuvwxyz{|}~\r\n");
    
    // Grid test
    term_input_string("\r\n\x1b[1mGrid Test (80 columns):\x1b[0m\r\n");
    // Exactly 80 characters (0-79 positions)
    term_input_string("01234567890123456789012345678901234567890123456789012345678901234567890123456789\r\n");
    term_input_string("          1         2         3         4         5         6         7         \r\n");
    
    term_input_string("\r\n\x1b[32m>>> Ready for input! <<<\x1b[0m\r\n");
}

void test_mode_handle_input(const uint8_t* data, size_t len) {
    // Echo back characters (simulate a basic shell echo)
    for (size_t i = 0; i < len; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
            // Echo printable characters back to terminal
            term_input_bytes(&data[i], 1);
        } else if (data[i] == '\r') {
            // Echo carriage return + line feed
            const char* crlf = "\r\n";
            term_input_bytes((const uint8_t*)crlf, 2);
        } else if (data[i] == '\t') {
            // Echo tab as spaces (4 spaces)
            const char* spaces = "    ";
            term_input_bytes((const uint8_t*)spaces, 4);
        } else if (data[i] == '\b') {
            // Backspace - send backspace + space + backspace to erase character
            const char* erase = "\b \b";
            term_input_bytes((const uint8_t*)erase, 3);
        } else if (data[i] == 127) {
            // DEL character (127) - treat as backspace for compatibility
            const char* erase = "\b \b";
            term_input_bytes((const uint8_t*)erase, 3);
        } else if (data[i] == 27 && i + 3 < len && 
                   data[i+1] == '[' && data[i+2] == '3' && data[i+3] == '~') {
            // DEL - show visual feedback
            const char* msg = " [DEL] ";
            term_input_bytes((const uint8_t*)msg, strlen(msg));
            i += 3; // Skip the rest of the escape sequence
        } else if (data[i] == 3) {
            // Ctrl+C - show visual feedback
            const char* msg = " [CTRL+C] ";
            term_input_bytes((const uint8_t*)msg, strlen(msg));
        } else if (data[i] == 27 && i + 1 < len && data[i+1] >= 'a' && data[i+1] <= 'z') {
            // Alt + lowercase letter combination (ESC + letter)
            char alt_msg[16];
            snprintf(alt_msg, sizeof(alt_msg), " [ALT+%c] ", data[i+1] - 'a' + 'A');
            term_input_bytes((const uint8_t*)alt_msg, strlen(alt_msg));
            i++; // Skip the next character since we processed it
        } else if (data[i] == 27 && i + 1 < len && data[i+1] >= 'A' && data[i+1] <= 'Z') {
            // Alt + uppercase letter combination (ESC + letter with shift)
            char alt_msg[16];
            snprintf(alt_msg, sizeof(alt_msg), " [ALT+SHIFT+%c] ", data[i+1]);
            term_input_bytes((const uint8_t*)alt_msg, strlen(alt_msg));
            i++; // Skip the next character since we processed it
        } else if (data[i] == 27 && (i + 2 >= len || data[i+1] != '[')) {
            // ESC - show visual feedback only if it's not part of an escape sequence
            // Most terminal escape sequences start with ESC[ so we avoid those
            const char* msg = " [ESC] ";
            term_input_bytes((const uint8_t*)msg, strlen(msg));
        } else if (data[i] == 27) {
            // This is likely an escape sequence, pass it through normally
            term_input_bytes(&data[i], 1);
        } else if (data[i] < 32) {
            // Other control characters - show as [CTRL+X]
            char ctrl_msg[16];
            snprintf(ctrl_msg, sizeof(ctrl_msg), " [CTRL+%c] ", 'A' + data[i] - 1);
            term_input_bytes((const uint8_t*)ctrl_msg, strlen(ctrl_msg));
        }
    }
}

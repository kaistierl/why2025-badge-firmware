/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "keyboard.h"

static keyboard_ascii_value_t const keyboard_map[KEY_SCANCODE_COUNT] = {
    [KEY_SCANCODE_UNKNOWN] = {0, 0},

    // Letters
    [KEY_SCANCODE_A] = {'a', 'A'},
    [KEY_SCANCODE_B] = {'b', 'B'},
    [KEY_SCANCODE_C] = {'c', 'C'},
    [KEY_SCANCODE_D] = {'d', 'D'},
    [KEY_SCANCODE_E] = {'e', 'E'},
    [KEY_SCANCODE_F] = {'f', 'F'},
    [KEY_SCANCODE_G] = {'g', 'G'},
    [KEY_SCANCODE_H] = {'h', 'H'},
    [KEY_SCANCODE_I] = {'i', 'I'},
    [KEY_SCANCODE_J] = {'j', 'J'},
    [KEY_SCANCODE_K] = {'k', 'K'},
    [KEY_SCANCODE_L] = {'l', 'L'},
    [KEY_SCANCODE_M] = {'m', 'M'},
    [KEY_SCANCODE_N] = {'n', 'N'},
    [KEY_SCANCODE_O] = {'o', 'O'},
    [KEY_SCANCODE_P] = {'p', 'P'},
    [KEY_SCANCODE_Q] = {'q', 'Q'},
    [KEY_SCANCODE_R] = {'r', 'R'},
    [KEY_SCANCODE_S] = {'s', 'S'},
    [KEY_SCANCODE_T] = {'t', 'T'},
    [KEY_SCANCODE_U] = {'u', 'U'},
    [KEY_SCANCODE_V] = {'v', 'V'},
    [KEY_SCANCODE_W] = {'w', 'W'},
    [KEY_SCANCODE_X] = {'x', 'X'},
    [KEY_SCANCODE_Y] = {'y', 'Y'},
    [KEY_SCANCODE_Z] = {'z', 'Z'},

    // Numbers
    [KEY_SCANCODE_1] = {'1', '!'},
    [KEY_SCANCODE_2] = {'2', '@'},
    [KEY_SCANCODE_3] = {'3', '#'},
    [KEY_SCANCODE_4] = {'4', '$'},
    [KEY_SCANCODE_5] = {'5', '%'},
    [KEY_SCANCODE_6] = {'6', '^'},
    [KEY_SCANCODE_7] = {'7', '&'},
    [KEY_SCANCODE_8] = {'8', '*'},
    [KEY_SCANCODE_9] = {'9', '('},
    [KEY_SCANCODE_0] = {'0', ')'},

    // Control keys (non-printable)
    [KEY_SCANCODE_RETURN]    = {'\r', '\r'},
    [KEY_SCANCODE_ESCAPE]    = {0, 0},
    [KEY_SCANCODE_BACKSPACE] = {'\b', '\b'},
    [KEY_SCANCODE_TAB]       = {'\t', '\t'},
    [KEY_SCANCODE_SPACE]     = {' ', ' '},

    // Punctuation
    [KEY_SCANCODE_MINUS]        = {'-', '_'},
    [KEY_SCANCODE_EQUALS]       = {'=', '+'},
    [KEY_SCANCODE_LEFTBRACKET]  = {'[', '{'},
    [KEY_SCANCODE_RIGHTBRACKET] = {']', '}'},
    [KEY_SCANCODE_BACKSLASH]    = {'\\', '|'},
    [KEY_SCANCODE_NONUSHASH]    = {'\\', '|'}, // Same as backslash on US keyboards
    [KEY_SCANCODE_SEMICOLON]    = {';', ':'},
    [KEY_SCANCODE_APOSTROPHE]   = {'\'', '"'},
    [KEY_SCANCODE_GRAVE]        = {'`', '~'},
    [KEY_SCANCODE_COMMA]        = {',', '<'},
    [KEY_SCANCODE_PERIOD]       = {'.', '>'},
    [KEY_SCANCODE_SLASH]        = {'/', '?'},

    // Lock keys (non-printable)
    [KEY_SCANCODE_CAPSLOCK] = {0, 0},

    // Function keys (non-printable)
    [KEY_SCANCODE_F1]  = {0, 0},
    [KEY_SCANCODE_F2]  = {0, 0},
    [KEY_SCANCODE_F3]  = {0, 0},
    [KEY_SCANCODE_F4]  = {0, 0},
    [KEY_SCANCODE_F5]  = {0, 0},
    [KEY_SCANCODE_F6]  = {0, 0},
    [KEY_SCANCODE_F7]  = {0, 0},
    [KEY_SCANCODE_F8]  = {0, 0},
    [KEY_SCANCODE_F9]  = {0, 0},
    [KEY_SCANCODE_F10] = {0, 0},
    [KEY_SCANCODE_F11] = {0, 0},
    [KEY_SCANCODE_F12] = {0, 0},

    // Navigation keys (non-printable)
    [KEY_SCANCODE_PRINTSCREEN] = {0, 0},
    [KEY_SCANCODE_SCROLLLOCK]  = {0, 0},
    [KEY_SCANCODE_PAUSE]       = {0, 0},
    [KEY_SCANCODE_INSERT]      = {0, 0},
    [KEY_SCANCODE_HOME]        = {0, 0},
    [KEY_SCANCODE_PAGEUP]      = {0, 0},
    [KEY_SCANCODE_DELETE]      = {0, 0},
    [KEY_SCANCODE_END]         = {0, 0},
    [KEY_SCANCODE_PAGEDOWN]    = {0, 0},
    [KEY_SCANCODE_RIGHT]       = {0, 0},
    [KEY_SCANCODE_LEFT]        = {0, 0},
    [KEY_SCANCODE_DOWN]        = {0, 0},
    [KEY_SCANCODE_UP]          = {0, 0},

    // Keypad
    [KEY_SCANCODE_NUMLOCKCLEAR] = {0, 0},
    [KEY_SCANCODE_KP_DIVIDE]    = {'/', '/'},
    [KEY_SCANCODE_KP_MULTIPLY]  = {'*', '*'},
    [KEY_SCANCODE_KP_MINUS]     = {'-', '-'},
    [KEY_SCANCODE_KP_PLUS]      = {'+', '+'},
    [KEY_SCANCODE_KP_ENTER]     = {'\r', '\r'},
    [KEY_SCANCODE_KP_1]         = {'1', '1'},
    [KEY_SCANCODE_KP_2]         = {'2', '2'},
    [KEY_SCANCODE_KP_3]         = {'3', '3'},
    [KEY_SCANCODE_KP_4]         = {'4', '4'},
    [KEY_SCANCODE_KP_5]         = {'5', '5'},
    [KEY_SCANCODE_KP_6]         = {'6', '6'},
    [KEY_SCANCODE_KP_7]         = {'7', '7'},
    [KEY_SCANCODE_KP_8]         = {'8', '8'},
    [KEY_SCANCODE_KP_9]         = {'9', '9'},
    [KEY_SCANCODE_KP_0]         = {'0', '0'},
    [KEY_SCANCODE_KP_PERIOD]    = {'.', '.'},

    // ISO keyboard additional key
    [KEY_SCANCODE_NONUSBACKSLASH] = {'\\', '|'},

    // Special keys (non-printable)
    [KEY_SCANCODE_APPLICATION] = {0, 0},
    [KEY_SCANCODE_POWER]       = {0, 0},
    [KEY_SCANCODE_KP_EQUALS]   = {'=', '='},

    // Additional function keys (non-printable)
    [KEY_SCANCODE_F13] = {0, 0},
    [KEY_SCANCODE_F14] = {0, 0},
    [KEY_SCANCODE_F15] = {0, 0},
    [KEY_SCANCODE_F16] = {0, 0},
    [KEY_SCANCODE_F17] = {0, 0},
    [KEY_SCANCODE_F18] = {0, 0},
    [KEY_SCANCODE_F19] = {0, 0},
    [KEY_SCANCODE_F20] = {0, 0},
    [KEY_SCANCODE_F21] = {0, 0},
    [KEY_SCANCODE_F22] = {0, 0},
    [KEY_SCANCODE_F23] = {0, 0},
    [KEY_SCANCODE_F24] = {0, 0},

    // System keys (non-printable)
    [KEY_SCANCODE_EXECUTE]    = {0, 0},
    [KEY_SCANCODE_HELP]       = {0, 0},
    [KEY_SCANCODE_MENU]       = {0, 0},
    [KEY_SCANCODE_SELECT]     = {0, 0},
    [KEY_SCANCODE_STOP]       = {0, 0},
    [KEY_SCANCODE_AGAIN]      = {0, 0},
    [KEY_SCANCODE_UNDO]       = {0, 0},
    [KEY_SCANCODE_CUT]        = {0, 0},
    [KEY_SCANCODE_COPY]       = {0, 0},
    [KEY_SCANCODE_PASTE]      = {0, 0},
    [KEY_SCANCODE_FIND]       = {0, 0},
    [KEY_SCANCODE_MUTE]       = {0, 0},
    [KEY_SCANCODE_VOLUMEUP]   = {0, 0},
    [KEY_SCANCODE_VOLUMEDOWN] = {0, 0},

    // Additional keypad keys
    [KEY_SCANCODE_KP_COMMA] = {',', ','},

    // Modifier keys (non-printable)
    [KEY_SCANCODE_LCTRL]  = {0, 0},
    [KEY_SCANCODE_LSHIFT] = {0, 0},
    [KEY_SCANCODE_LALT]   = {0, 0},
    [KEY_SCANCODE_LGUI]   = {0, 0},
    [KEY_SCANCODE_RCTRL]  = {0, 0},
    [KEY_SCANCODE_RSHIFT] = {0, 0},
    [KEY_SCANCODE_RALT]   = {0, 0},
    [KEY_SCANCODE_RGUI]   = {0, 0},

    // All other scancodes default to { 0, 0 } due to static initialization
};

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

#include "../event.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t key_code_t;
typedef uint16_t key_mod_t;

// Most of this is from SDL3

typedef enum {
    KEY_SCANCODE_UNKNOWN = 0,

    /**
     *  \name Usage page 0x07
     *
     *  These values are from usage page 0x07 (USB keyboard page).
     */
    /* @{ */

    KEY_SCANCODE_A = 4,
    KEY_SCANCODE_B = 5,
    KEY_SCANCODE_C = 6,
    KEY_SCANCODE_D = 7,
    KEY_SCANCODE_E = 8,
    KEY_SCANCODE_F = 9,
    KEY_SCANCODE_G = 10,
    KEY_SCANCODE_H = 11,
    KEY_SCANCODE_I = 12,
    KEY_SCANCODE_J = 13,
    KEY_SCANCODE_K = 14,
    KEY_SCANCODE_L = 15,
    KEY_SCANCODE_M = 16,
    KEY_SCANCODE_N = 17,
    KEY_SCANCODE_O = 18,
    KEY_SCANCODE_P = 19,
    KEY_SCANCODE_Q = 20,
    KEY_SCANCODE_R = 21,
    KEY_SCANCODE_S = 22,
    KEY_SCANCODE_T = 23,
    KEY_SCANCODE_U = 24,
    KEY_SCANCODE_V = 25,
    KEY_SCANCODE_W = 26,
    KEY_SCANCODE_X = 27,
    KEY_SCANCODE_Y = 28,
    KEY_SCANCODE_Z = 29,

    KEY_SCANCODE_1 = 30,
    KEY_SCANCODE_2 = 31,
    KEY_SCANCODE_3 = 32,
    KEY_SCANCODE_4 = 33,
    KEY_SCANCODE_5 = 34,
    KEY_SCANCODE_6 = 35,
    KEY_SCANCODE_7 = 36,
    KEY_SCANCODE_8 = 37,
    KEY_SCANCODE_9 = 38,
    KEY_SCANCODE_0 = 39,

    KEY_SCANCODE_RETURN    = 40,
    KEY_SCANCODE_ESCAPE    = 41,
    KEY_SCANCODE_BACKSPACE = 42,
    KEY_SCANCODE_TAB       = 43,
    KEY_SCANCODE_SPACE     = 44,

    KEY_SCANCODE_MINUS        = 45,
    KEY_SCANCODE_EQUALS       = 46,
    KEY_SCANCODE_LEFTBRACKET  = 47,
    KEY_SCANCODE_RIGHTBRACKET = 48,
    KEY_SCANCODE_BACKSLASH    = 49, /**< Located at the lower left of the return
                                     *   key on ISO keyboards and at the right end
                                     *   of the QWERTY row on ANSI keyboards.
                                     *   Produces REVERSE SOLIDUS (backslash) and
                                     *   VERTICAL LINE in a US layout, REVERSE
                                     *   SOLIDUS and VERTICAL LINE in a UK Mac
                                     *   layout, NUMBER SIGN and TILDE in a UK
                                     *   Windows layout, DOLLAR SIGN and POUND SIGN
                                     *   in a Swiss German layout, NUMBER SIGN and
                                     *   APOSTROPHE in a German layout, GRAVE
                                     *   ACCENT and POUND SIGN in a French Mac
                                     *   layout, and ASTERISK and MICRO SIGN in a
                                     *   French Windows layout.
                                     */
    KEY_SCANCODE_NONUSHASH    = 50, /**< ISO USB keyboards actually use this code
                                     *   instead of 49 for the same key, but all
                                     *   OSes I've seen treat the two codes
                                     *   identically. So, as an implementor, unless
                                     *   your keyboard generates both of those
                                     *   codes and your OS treats them differently,
                                     *   you should generate KEY_SCANCODE_BACKSLASH
                                     *   instead of this code. As a user, you
                                     *   should not rely on this code because SDL
                                     *   will never generate it with most (all?)
                                     *   keyboards.
                                     */
    KEY_SCANCODE_SEMICOLON    = 51,
    KEY_SCANCODE_APOSTROPHE   = 52,
    KEY_SCANCODE_GRAVE        = 53, /**< Located in the top left corner (on both ANSI
                                     *   and ISO keyboards). Produces GRAVE ACCENT and
                                     *   TILDE in a US Windows layout and in US and UK
                                     *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                                     *   and NOT SIGN in a UK Windows layout, SECTION
                                     *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                                     *   layouts on ISO keyboards, SECTION SIGN and
                                     *   DEGREE SIGN in a Swiss German layout (Mac:
                                     *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                                     *   DEGREE SIGN in a German layout (Mac: only on
                                     *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                                     *   French Windows layout, COMMERCIAL AT and
                                     *   NUMBER SIGN in a French Mac layout on ISO
                                     *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                                     *   SIGN in a Swiss German, German, or French Mac
                                     *   layout on ANSI keyboards.
                                     */
    KEY_SCANCODE_COMMA        = 54,
    KEY_SCANCODE_PERIOD       = 55,
    KEY_SCANCODE_SLASH        = 56,

    KEY_SCANCODE_CAPSLOCK = 57,

    KEY_SCANCODE_F1  = 58,
    KEY_SCANCODE_F2  = 59,
    KEY_SCANCODE_F3  = 60,
    KEY_SCANCODE_F4  = 61,
    KEY_SCANCODE_F5  = 62,
    KEY_SCANCODE_F6  = 63,
    KEY_SCANCODE_F7  = 64,
    KEY_SCANCODE_F8  = 65,
    KEY_SCANCODE_F9  = 66,
    KEY_SCANCODE_F10 = 67,
    KEY_SCANCODE_F11 = 68,
    KEY_SCANCODE_F12 = 69,

    KEY_SCANCODE_PRINTSCREEN = 70,
    KEY_SCANCODE_SCROLLLOCK  = 71,
    KEY_SCANCODE_PAUSE       = 72,
    KEY_SCANCODE_INSERT      = 73, /**< insert on PC, help on some Mac keyboards (but
                                        does send code 73, not 117) */
    KEY_SCANCODE_HOME        = 74,
    KEY_SCANCODE_PAGEUP      = 75,
    KEY_SCANCODE_DELETE      = 76,
    KEY_SCANCODE_END         = 77,
    KEY_SCANCODE_PAGEDOWN    = 78,
    KEY_SCANCODE_RIGHT       = 79,
    KEY_SCANCODE_LEFT        = 80,
    KEY_SCANCODE_DOWN        = 81,
    KEY_SCANCODE_UP          = 82,

    KEY_SCANCODE_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
                                     */
    KEY_SCANCODE_KP_DIVIDE    = 84,
    KEY_SCANCODE_KP_MULTIPLY  = 85,
    KEY_SCANCODE_KP_MINUS     = 86,
    KEY_SCANCODE_KP_PLUS      = 87,
    KEY_SCANCODE_KP_ENTER     = 88,
    KEY_SCANCODE_KP_1         = 89,
    KEY_SCANCODE_KP_2         = 90,
    KEY_SCANCODE_KP_3         = 91,
    KEY_SCANCODE_KP_4         = 92,
    KEY_SCANCODE_KP_5         = 93,
    KEY_SCANCODE_KP_6         = 94,
    KEY_SCANCODE_KP_7         = 95,
    KEY_SCANCODE_KP_8         = 96,
    KEY_SCANCODE_KP_9         = 97,
    KEY_SCANCODE_KP_0         = 98,
    KEY_SCANCODE_KP_PERIOD    = 99,

    KEY_SCANCODE_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
                                        *   keyboards have over ANSI ones,
                                        *   located between left shift and Y.
                                        *   Produces GRAVE ACCENT and TILDE in a
                                        *   US or UK Mac layout, REVERSE SOLIDUS
                                        *   (backslash) and VERTICAL LINE in a
                                        *   US or UK Windows layout, and
                                        *   LESS-THAN SIGN and GREATER-THAN SIGN
                                        *   in a Swiss German, German, or French
                                        *   layout. */
    KEY_SCANCODE_APPLICATION    = 101, /**< windows contextual menu, compose */
    KEY_SCANCODE_POWER          = 102, /**< The USB document says this is a status flag,
                                        *   not a physical key - but some Mac keyboards
                                        *   do have a power key. */
    KEY_SCANCODE_KP_EQUALS      = 103,
    KEY_SCANCODE_F13            = 104,
    KEY_SCANCODE_F14            = 105,
    KEY_SCANCODE_F15            = 106,
    KEY_SCANCODE_F16            = 107,
    KEY_SCANCODE_F17            = 108,
    KEY_SCANCODE_F18            = 109,
    KEY_SCANCODE_F19            = 110,
    KEY_SCANCODE_F20            = 111,
    KEY_SCANCODE_F21            = 112,
    KEY_SCANCODE_F22            = 113,
    KEY_SCANCODE_F23            = 114,
    KEY_SCANCODE_F24            = 115,
    KEY_SCANCODE_EXECUTE        = 116,
    KEY_SCANCODE_HELP           = 117, /**< AL Integrated Help Center */
    KEY_SCANCODE_MENU           = 118, /**< Menu (show menu) */
    KEY_SCANCODE_SELECT         = 119,
    KEY_SCANCODE_STOP           = 120, /**< AC Stop */
    KEY_SCANCODE_AGAIN          = 121, /**< AC Redo/Repeat */
    KEY_SCANCODE_UNDO           = 122, /**< AC Undo */
    KEY_SCANCODE_CUT            = 123, /**< AC Cut */
    KEY_SCANCODE_COPY           = 124, /**< AC Copy */
    KEY_SCANCODE_PASTE          = 125, /**< AC Paste */
    KEY_SCANCODE_FIND           = 126, /**< AC Find */
    KEY_SCANCODE_MUTE           = 127,
    KEY_SCANCODE_VOLUMEUP       = 128,
    KEY_SCANCODE_VOLUMEDOWN     = 129,
    /* not sure whether there's a reason to enable these */
    /*     KEY_SCANCODE_LOCKINGCAPSLOCK = 130,  */
    /*     KEY_SCANCODE_LOCKINGNUMLOCK = 131, */
    /*     KEY_SCANCODE_LOCKINGSCROLLLOCK = 132, */
    KEY_SCANCODE_KP_COMMA       = 133,
    KEY_SCANCODE_KP_EQUALSAS400 = 134,

    KEY_SCANCODE_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    KEY_SCANCODE_INTERNATIONAL2 = 136,
    KEY_SCANCODE_INTERNATIONAL3 = 137, /**< Yen */
    KEY_SCANCODE_INTERNATIONAL4 = 138,
    KEY_SCANCODE_INTERNATIONAL5 = 139,
    KEY_SCANCODE_INTERNATIONAL6 = 140,
    KEY_SCANCODE_INTERNATIONAL7 = 141,
    KEY_SCANCODE_INTERNATIONAL8 = 142,
    KEY_SCANCODE_INTERNATIONAL9 = 143,
    KEY_SCANCODE_LANG1          = 144, /**< Hangul/English toggle */
    KEY_SCANCODE_LANG2          = 145, /**< Hanja conversion */
    KEY_SCANCODE_LANG3          = 146, /**< Katakana */
    KEY_SCANCODE_LANG4          = 147, /**< Hiragana */
    KEY_SCANCODE_LANG5          = 148, /**< Zenkaku/Hankaku */
    KEY_SCANCODE_LANG6          = 149, /**< reserved */
    KEY_SCANCODE_LANG7          = 150, /**< reserved */
    KEY_SCANCODE_LANG8          = 151, /**< reserved */
    KEY_SCANCODE_LANG9          = 152, /**< reserved */

    KEY_SCANCODE_ALTERASE   = 153, /**< Erase-Eaze */
    KEY_SCANCODE_SYSREQ     = 154,
    KEY_SCANCODE_CANCEL     = 155, /**< AC Cancel */
    KEY_SCANCODE_CLEAR      = 156,
    KEY_SCANCODE_PRIOR      = 157,
    KEY_SCANCODE_RETURN2    = 158,
    KEY_SCANCODE_SEPARATOR  = 159,
    KEY_SCANCODE_OUT        = 160,
    KEY_SCANCODE_OPER       = 161,
    KEY_SCANCODE_CLEARAGAIN = 162,
    KEY_SCANCODE_CRSEL      = 163,
    KEY_SCANCODE_EXSEL      = 164,

    KEY_SCANCODE_KP_00              = 176,
    KEY_SCANCODE_KP_000             = 177,
    KEY_SCANCODE_THOUSANDSSEPARATOR = 178,
    KEY_SCANCODE_DECIMALSEPARATOR   = 179,
    KEY_SCANCODE_CURRENCYUNIT       = 180,
    KEY_SCANCODE_CURRENCYSUBUNIT    = 181,
    KEY_SCANCODE_KP_LEFTPAREN       = 182,
    KEY_SCANCODE_KP_RIGHTPAREN      = 183,
    KEY_SCANCODE_KP_LEFTBRACE       = 184,
    KEY_SCANCODE_KP_RIGHTBRACE      = 185,
    KEY_SCANCODE_KP_TAB             = 186,
    KEY_SCANCODE_KP_BACKSPACE       = 187,
    KEY_SCANCODE_KP_A               = 188,
    KEY_SCANCODE_KP_B               = 189,
    KEY_SCANCODE_KP_C               = 190,
    KEY_SCANCODE_KP_D               = 191,
    KEY_SCANCODE_KP_E               = 192,
    KEY_SCANCODE_KP_F               = 193,
    KEY_SCANCODE_KP_XOR             = 194,
    KEY_SCANCODE_KP_POWER           = 195,
    KEY_SCANCODE_KP_PERCENT         = 196,
    KEY_SCANCODE_KP_LESS            = 197,
    KEY_SCANCODE_KP_GREATER         = 198,
    KEY_SCANCODE_KP_AMPERSAND       = 199,
    KEY_SCANCODE_KP_DBLAMPERSAND    = 200,
    KEY_SCANCODE_KP_VERTICALBAR     = 201,
    KEY_SCANCODE_KP_DBLVERTICALBAR  = 202,
    KEY_SCANCODE_KP_COLON           = 203,
    KEY_SCANCODE_KP_HASH            = 204,
    KEY_SCANCODE_KP_SPACE           = 205,
    KEY_SCANCODE_KP_AT              = 206,
    KEY_SCANCODE_KP_EXCLAM          = 207,
    KEY_SCANCODE_KP_MEMSTORE        = 208,
    KEY_SCANCODE_KP_MEMRECALL       = 209,
    KEY_SCANCODE_KP_MEMCLEAR        = 210,
    KEY_SCANCODE_KP_MEMADD          = 211,
    KEY_SCANCODE_KP_MEMSUBTRACT     = 212,
    KEY_SCANCODE_KP_MEMMULTIPLY     = 213,
    KEY_SCANCODE_KP_MEMDIVIDE       = 214,
    KEY_SCANCODE_KP_PLUSMINUS       = 215,
    KEY_SCANCODE_KP_CLEAR           = 216,
    KEY_SCANCODE_KP_CLEARENTRY      = 217,
    KEY_SCANCODE_KP_BINARY          = 218,
    KEY_SCANCODE_KP_OCTAL           = 219,
    KEY_SCANCODE_KP_DECIMAL         = 220,
    KEY_SCANCODE_KP_HEXADECIMAL     = 221,

    KEY_SCANCODE_LCTRL  = 224,
    KEY_SCANCODE_LSHIFT = 225,
    KEY_SCANCODE_LALT   = 226, /**< alt, option */
    KEY_SCANCODE_LGUI   = 227, /**< windows, command (apple), meta */
    KEY_SCANCODE_RCTRL  = 228,
    KEY_SCANCODE_RSHIFT = 229,
    KEY_SCANCODE_RALT   = 230, /**< alt gr, option */
    KEY_SCANCODE_RGUI   = 231, /**< windows, command (apple), meta */

    KEY_SCANCODE_MODE = 257, /**< I'm not sure if this is really not covered
                              *   by any of the above, but since there's a
                              *   special KMOD_MODE for it I'm adding it here
                              */

    /* @} */ /* Usage page 0x07 */

    /**
     *  \name Usage page 0x0C
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     *
     *  There are way more keys in the spec than we can represent in the
     *  current scancode range, so pick the ones that commonly come up in
     *  real world usage.
     */
    /* @{ */

    KEY_SCANCODE_SLEEP = 258, /**< Sleep */
    KEY_SCANCODE_WAKE  = 259, /**< Wake */

    KEY_SCANCODE_CHANNEL_INCREMENT = 260, /**< Channel Increment */
    KEY_SCANCODE_CHANNEL_DECREMENT = 261, /**< Channel Decrement */

    KEY_SCANCODE_MEDIA_PLAY           = 262, /**< Play */
    KEY_SCANCODE_MEDIA_PAUSE          = 263, /**< Pause */
    KEY_SCANCODE_MEDIA_RECORD         = 264, /**< Record */
    KEY_SCANCODE_MEDIA_FAST_FORWARD   = 265, /**< Fast Forward */
    KEY_SCANCODE_MEDIA_REWIND         = 266, /**< Rewind */
    KEY_SCANCODE_MEDIA_NEXT_TRACK     = 267, /**< Next Track */
    KEY_SCANCODE_MEDIA_PREVIOUS_TRACK = 268, /**< Previous Track */
    KEY_SCANCODE_MEDIA_STOP           = 269, /**< Stop */
    KEY_SCANCODE_MEDIA_EJECT          = 270, /**< Eject */
    KEY_SCANCODE_MEDIA_PLAY_PAUSE     = 271, /**< Play / Pause */
    KEY_SCANCODE_MEDIA_SELECT         = 272, /* Media Select */

    KEY_SCANCODE_AC_NEW        = 273, /**< AC New */
    KEY_SCANCODE_AC_OPEN       = 274, /**< AC Open */
    KEY_SCANCODE_AC_CLOSE      = 275, /**< AC Close */
    KEY_SCANCODE_AC_EXIT       = 276, /**< AC Exit */
    KEY_SCANCODE_AC_SAVE       = 277, /**< AC Save */
    KEY_SCANCODE_AC_PRINT      = 278, /**< AC Print */
    KEY_SCANCODE_AC_PROPERTIES = 279, /**< AC Properties */

    KEY_SCANCODE_AC_SEARCH    = 280, /**< AC Search */
    KEY_SCANCODE_AC_HOME      = 281, /**< AC Home */
    KEY_SCANCODE_AC_BACK      = 282, /**< AC Back */
    KEY_SCANCODE_AC_FORWARD   = 283, /**< AC Forward */
    KEY_SCANCODE_AC_STOP      = 284, /**< AC Stop */
    KEY_SCANCODE_AC_REFRESH   = 285, /**< AC Refresh */
    KEY_SCANCODE_AC_BOOKMARKS = 286, /**< AC Bookmarks */

    /* @} */ /* Usage page 0x0C */


    /**
     *  \name Mobile keys
     *
     *  These are values that are often used on mobile phones.
     */
    /* @{ */

    KEY_SCANCODE_SOFTLEFT  = 287, /**< Usually situated below the display on phones and
                                       used as a multi-function feature key for selecting
                                       a software defined function shown on the bottom left
                                       of the display. */
    KEY_SCANCODE_SOFTRIGHT = 288, /**< Usually situated below the display on phones and
                                       used as a multi-function feature key for selecting
                                       a software defined function shown on the bottom right
                                       of the display. */
    KEY_SCANCODE_CALL      = 289, /**< Used for accepting phone calls. */
    KEY_SCANCODE_ENDCALL   = 290, /**< Used for rejecting phone calls. */

    /* @} */ /* Mobile keys */

    /* Add any other keys here. */

    KEY_SCANCODE_FN       = 291,
    KEY_SCANCODE_SQUARE   = 292,
    KEY_SCANCODE_TRIANGLE = 293,
    KEY_SCANCODE_CROSS    = 294,
    KEY_SCANCODE_CIRCLE   = 295,
    KEY_SCANCODE_CLOUD    = 296,
    KEY_SCANCODE_DIAMOND  = 297,

    KEY_SCANCODE_RESERVED = 400, /**< 400-500 reserved for dynamic keycodes */

    KEY_SCANCODE_COUNT = 512 /**< not a key, just marks the number of scancodes for array bounds */

} keyboard_scancode_t;


#define KEY_EXTENDED_MASK        (1u << 29)
#define KEY_SCANCODE_MASK        (1u << 30)
#define SCANCODE_TO_KEYCODE(X)   (X | KEY_SCANCODE_MASK)
#define KEY_UNKNOWN              0x00000000u /**< 0 */
#define KEY_RETURN               0x0000000du /**< '\r' */
#define KEY_ESCAPE               0x0000001bu /**< '\x1B' */
#define KEY_BACKSPACE            0x00000008u /**< '\b' */
#define KEY_TAB                  0x00000009u /**< '\t' */
#define KEY_SPACE                0x00000020u /**< ' ' */
#define KEY_EXCLAIM              0x00000021u /**< '!' */
#define KEY_DBLAPOSTROPHE        0x00000022u /**< '"' */
#define KEY_HASH                 0x00000023u /**< '#' */
#define KEY_DOLLAR               0x00000024u /**< '$' */
#define KEY_PERCENT              0x00000025u /**< '%' */
#define KEY_AMPERSAND            0x00000026u /**< '&' */
#define KEY_APOSTROPHE           0x00000027u /**< '\'' */
#define KEY_LEFTPAREN            0x00000028u /**< '(' */
#define KEY_RIGHTPAREN           0x00000029u /**< ')' */
#define KEY_ASTERISK             0x0000002au /**< '*' */
#define KEY_PLUS                 0x0000002bu /**< '+' */
#define KEY_COMMA                0x0000002cu /**< ',' */
#define KEY_MINUS                0x0000002du /**< '-' */
#define KEY_PERIOD               0x0000002eu /**< '.' */
#define KEY_SLASH                0x0000002fu /**< '/' */
#define KEY_0                    0x00000030u /**< '0' */
#define KEY_1                    0x00000031u /**< '1' */
#define KEY_2                    0x00000032u /**< '2' */
#define KEY_3                    0x00000033u /**< '3' */
#define KEY_4                    0x00000034u /**< '4' */
#define KEY_5                    0x00000035u /**< '5' */
#define KEY_6                    0x00000036u /**< '6' */
#define KEY_7                    0x00000037u /**< '7' */
#define KEY_8                    0x00000038u /**< '8' */
#define KEY_9                    0x00000039u /**< '9' */
#define KEY_COLON                0x0000003au /**< ':' */
#define KEY_SEMICOLON            0x0000003bu /**< ';' */
#define KEY_LESS                 0x0000003cu /**< '<' */
#define KEY_EQUALS               0x0000003du /**< '=' */
#define KEY_GREATER              0x0000003eu /**< '>' */
#define KEY_QUESTION             0x0000003fu /**< '?' */
#define KEY_AT                   0x00000040u /**< '@' */
#define KEY_LEFTBRACKET          0x0000005bu /**< '[' */
#define KEY_BACKSLASH            0x0000005cu /**< '\\' */
#define KEY_RIGHTBRACKET         0x0000005du /**< ']' */
#define KEY_CARET                0x0000005eu /**< '^' */
#define KEY_UNDERSCORE           0x0000005fu /**< '_' */
#define KEY_GRAVE                0x00000060u /**< '`' */
#define KEY_A                    0x00000061u /**< 'a' */
#define KEY_B                    0x00000062u /**< 'b' */
#define KEY_C                    0x00000063u /**< 'c' */
#define KEY_D                    0x00000064u /**< 'd' */
#define KEY_E                    0x00000065u /**< 'e' */
#define KEY_F                    0x00000066u /**< 'f' */
#define KEY_G                    0x00000067u /**< 'g' */
#define KEY_H                    0x00000068u /**< 'h' */
#define KEY_I                    0x00000069u /**< 'i' */
#define KEY_J                    0x0000006au /**< 'j' */
#define KEY_K                    0x0000006bu /**< 'k' */
#define KEY_L                    0x0000006cu /**< 'l' */
#define KEY_M                    0x0000006du /**< 'm' */
#define KEY_N                    0x0000006eu /**< 'n' */
#define KEY_O                    0x0000006fu /**< 'o' */
#define KEY_P                    0x00000070u /**< 'p' */
#define KEY_Q                    0x00000071u /**< 'q' */
#define KEY_R                    0x00000072u /**< 'r' */
#define KEY_S                    0x00000073u /**< 's' */
#define KEY_T                    0x00000074u /**< 't' */
#define KEY_U                    0x00000075u /**< 'u' */
#define KEY_V                    0x00000076u /**< 'v' */
#define KEY_W                    0x00000077u /**< 'w' */
#define KEY_X                    0x00000078u /**< 'x' */
#define KEY_Y                    0x00000079u /**< 'y' */
#define KEY_Z                    0x0000007au /**< 'z' */
#define KEY_LEFTBRACE            0x0000007bu /**< '{' */
#define KEY_PIPE                 0x0000007cu /**< '|' */
#define KEY_RIGHTBRACE           0x0000007du /**< '}' */
#define KEY_TILDE                0x0000007eu /**< '~' */
#define KEY_DELETE               0x0000007fu /**< '\x7F' */
#define KEY_PLUSMINUS            0x000000b1u /**< '\xB1' */
#define KEY_CAPSLOCK             0x40000039u /**< SCANCODE_TO_KEYCODE(SCANCODE_CAPSLOCK) */
#define KEY_F1                   0x4000003au /**< SCANCODE_TO_KEYCODE(SCANCODE_F1) */
#define KEY_F2                   0x4000003bu /**< SCANCODE_TO_KEYCODE(SCANCODE_F2) */
#define KEY_F3                   0x4000003cu /**< SCANCODE_TO_KEYCODE(SCANCODE_F3) */
#define KEY_F4                   0x4000003du /**< SCANCODE_TO_KEYCODE(SCANCODE_F4) */
#define KEY_F5                   0x4000003eu /**< SCANCODE_TO_KEYCODE(SCANCODE_F5) */
#define KEY_F6                   0x4000003fu /**< SCANCODE_TO_KEYCODE(SCANCODE_F6) */
#define KEY_F7                   0x40000040u /**< SCANCODE_TO_KEYCODE(SCANCODE_F7) */
#define KEY_F8                   0x40000041u /**< SCANCODE_TO_KEYCODE(SCANCODE_F8) */
#define KEY_F9                   0x40000042u /**< SCANCODE_TO_KEYCODE(SCANCODE_F9) */
#define KEY_F10                  0x40000043u /**< SCANCODE_TO_KEYCODE(SCANCODE_F10) */
#define KEY_F11                  0x40000044u /**< SCANCODE_TO_KEYCODE(SCANCODE_F11) */
#define KEY_F12                  0x40000045u /**< SCANCODE_TO_KEYCODE(SCANCODE_F12) */
#define KEY_PRINTSCREEN          0x40000046u /**< SCANCODE_TO_KEYCODE(SCANCODE_PRINTSCREEN) */
#define KEY_SCROLLLOCK           0x40000047u /**< SCANCODE_TO_KEYCODE(SCANCODE_SCROLLLOCK) */
#define KEY_PAUSE                0x40000048u /**< SCANCODE_TO_KEYCODE(SCANCODE_PAUSE) */
#define KEY_INSERT               0x40000049u /**< SCANCODE_TO_KEYCODE(SCANCODE_INSERT) */
#define KEY_HOME                 0x4000004au /**< SCANCODE_TO_KEYCODE(SCANCODE_HOME) */
#define KEY_PAGEUP               0x4000004bu /**< SCANCODE_TO_KEYCODE(SCANCODE_PAGEUP) */
#define KEY_END                  0x4000004du /**< SCANCODE_TO_KEYCODE(SCANCODE_END) */
#define KEY_PAGEDOWN             0x4000004eu /**< SCANCODE_TO_KEYCODE(SCANCODE_PAGEDOWN) */
#define KEY_RIGHT                0x4000004fu /**< SCANCODE_TO_KEYCODE(SCANCODE_RIGHT) */
#define KEY_LEFT                 0x40000050u /**< SCANCODE_TO_KEYCODE(SCANCODE_LEFT) */
#define KEY_DOWN                 0x40000051u /**< SCANCODE_TO_KEYCODE(SCANCODE_DOWN) */
#define KEY_UP                   0x40000052u /**< SCANCODE_TO_KEYCODE(SCANCODE_UP) */
#define KEY_NUMLOCKCLEAR         0x40000053u /**< SCANCODE_TO_KEYCODE(SCANCODE_NUMLOCKCLEAR) */
#define KEY_KP_DIVIDE            0x40000054u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_DIVIDE) */
#define KEY_KP_MULTIPLY          0x40000055u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MULTIPLY) */
#define KEY_KP_MINUS             0x40000056u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MINUS) */
#define KEY_KP_PLUS              0x40000057u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_PLUS) */
#define KEY_KP_ENTER             0x40000058u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_ENTER) */
#define KEY_KP_1                 0x40000059u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_1) */
#define KEY_KP_2                 0x4000005au /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_2) */
#define KEY_KP_3                 0x4000005bu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_3) */
#define KEY_KP_4                 0x4000005cu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_4) */
#define KEY_KP_5                 0x4000005du /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_5) */
#define KEY_KP_6                 0x4000005eu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_6) */
#define KEY_KP_7                 0x4000005fu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_7) */
#define KEY_KP_8                 0x40000060u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_8) */
#define KEY_KP_9                 0x40000061u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_9) */
#define KEY_KP_0                 0x40000062u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_0) */
#define KEY_KP_PERIOD            0x40000063u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_PERIOD) */
#define KEY_APPLICATION          0x40000065u /**< SCANCODE_TO_KEYCODE(SCANCODE_APPLICATION) */
#define KEY_POWER                0x40000066u /**< SCANCODE_TO_KEYCODE(SCANCODE_POWER) */
#define KEY_KP_EQUALS            0x40000067u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_EQUALS) */
#define KEY_F13                  0x40000068u /**< SCANCODE_TO_KEYCODE(SCANCODE_F13) */
#define KEY_F14                  0x40000069u /**< SCANCODE_TO_KEYCODE(SCANCODE_F14) */
#define KEY_F15                  0x4000006au /**< SCANCODE_TO_KEYCODE(SCANCODE_F15) */
#define KEY_F16                  0x4000006bu /**< SCANCODE_TO_KEYCODE(SCANCODE_F16) */
#define KEY_F17                  0x4000006cu /**< SCANCODE_TO_KEYCODE(SCANCODE_F17) */
#define KEY_F18                  0x4000006du /**< SCANCODE_TO_KEYCODE(SCANCODE_F18) */
#define KEY_F19                  0x4000006eu /**< SCANCODE_TO_KEYCODE(SCANCODE_F19) */
#define KEY_F20                  0x4000006fu /**< SCANCODE_TO_KEYCODE(SCANCODE_F20) */
#define KEY_F21                  0x40000070u /**< SCANCODE_TO_KEYCODE(SCANCODE_F21) */
#define KEY_F22                  0x40000071u /**< SCANCODE_TO_KEYCODE(SCANCODE_F22) */
#define KEY_F23                  0x40000072u /**< SCANCODE_TO_KEYCODE(SCANCODE_F23) */
#define KEY_F24                  0x40000073u /**< SCANCODE_TO_KEYCODE(SCANCODE_F24) */
#define KEY_EXECUTE              0x40000074u /**< SCANCODE_TO_KEYCODE(SCANCODE_EXECUTE) */
#define KEY_HELP                 0x40000075u /**< SCANCODE_TO_KEYCODE(SCANCODE_HELP) */
#define KEY_MENU                 0x40000076u /**< SCANCODE_TO_KEYCODE(SCANCODE_MENU) */
#define KEY_SELECT               0x40000077u /**< SCANCODE_TO_KEYCODE(SCANCODE_SELECT) */
#define KEY_STOP                 0x40000078u /**< SCANCODE_TO_KEYCODE(SCANCODE_STOP) */
#define KEY_AGAIN                0x40000079u /**< SCANCODE_TO_KEYCODE(SCANCODE_AGAIN) */
#define KEY_UNDO                 0x4000007au /**< SCANCODE_TO_KEYCODE(SCANCODE_UNDO) */
#define KEY_CUT                  0x4000007bu /**< SCANCODE_TO_KEYCODE(SCANCODE_CUT) */
#define KEY_COPY                 0x4000007cu /**< SCANCODE_TO_KEYCODE(SCANCODE_COPY) */
#define KEY_PASTE                0x4000007du /**< SCANCODE_TO_KEYCODE(SCANCODE_PASTE) */
#define KEY_FIND                 0x4000007eu /**< SCANCODE_TO_KEYCODE(SCANCODE_FIND) */
#define KEY_MUTE                 0x4000007fu /**< SCANCODE_TO_KEYCODE(SCANCODE_MUTE) */
#define KEY_VOLUMEUP             0x40000080u /**< SCANCODE_TO_KEYCODE(SCANCODE_VOLUMEUP) */
#define KEY_VOLUMEDOWN           0x40000081u /**< SCANCODE_TO_KEYCODE(SCANCODE_VOLUMEDOWN) */
#define KEY_KP_COMMA             0x40000085u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_COMMA) */
#define KEY_KP_EQUALSAS400       0x40000086u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_EQUALSAS400) */
#define KEY_ALTERASE             0x40000099u /**< SCANCODE_TO_KEYCODE(SCANCODE_ALTERASE) */
#define KEY_SYSREQ               0x4000009au /**< SCANCODE_TO_KEYCODE(SCANCODE_SYSREQ) */
#define KEY_CANCEL               0x4000009bu /**< SCANCODE_TO_KEYCODE(SCANCODE_CANCEL) */
#define KEY_CLEAR                0x4000009cu /**< SCANCODE_TO_KEYCODE(SCANCODE_CLEAR) */
#define KEY_PRIOR                0x4000009du /**< SCANCODE_TO_KEYCODE(SCANCODE_PRIOR) */
#define KEY_RETURN2              0x4000009eu /**< SCANCODE_TO_KEYCODE(SCANCODE_RETURN2) */
#define KEY_SEPARATOR            0x4000009fu /**< SCANCODE_TO_KEYCODE(SCANCODE_SEPARATOR) */
#define KEY_OUT                  0x400000a0u /**< SCANCODE_TO_KEYCODE(SCANCODE_OUT) */
#define KEY_OPER                 0x400000a1u /**< SCANCODE_TO_KEYCODE(SCANCODE_OPER) */
#define KEY_CLEARAGAIN           0x400000a2u /**< SCANCODE_TO_KEYCODE(SCANCODE_CLEARAGAIN) */
#define KEY_CRSEL                0x400000a3u /**< SCANCODE_TO_KEYCODE(SCANCODE_CRSEL) */
#define KEY_EXSEL                0x400000a4u /**< SCANCODE_TO_KEYCODE(SCANCODE_EXSEL) */
#define KEY_KP_00                0x400000b0u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_00) */
#define KEY_KP_000               0x400000b1u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_000) */
#define KEY_THOUSANDSSEPARATOR   0x400000b2u /**< SCANCODE_TO_KEYCODE(SCANCODE_THOUSANDSSEPARATOR) */
#define KEY_DECIMALSEPARATOR     0x400000b3u /**< SCANCODE_TO_KEYCODE(SCANCODE_DECIMALSEPARATOR) */
#define KEY_CURRENCYUNIT         0x400000b4u /**< SCANCODE_TO_KEYCODE(SCANCODE_CURRENCYUNIT) */
#define KEY_CURRENCYSUBUNIT      0x400000b5u /**< SCANCODE_TO_KEYCODE(SCANCODE_CURRENCYSUBUNIT) */
#define KEY_KP_LEFTPAREN         0x400000b6u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_LEFTPAREN) */
#define KEY_KP_RIGHTPAREN        0x400000b7u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_RIGHTPAREN) */
#define KEY_KP_LEFTBRACE         0x400000b8u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_LEFTBRACE) */
#define KEY_KP_RIGHTBRACE        0x400000b9u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_RIGHTBRACE) */
#define KEY_KP_TAB               0x400000bau /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_TAB) */
#define KEY_KP_BACKSPACE         0x400000bbu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_BACKSPACE) */
#define KEY_KP_A                 0x400000bcu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_A) */
#define KEY_KP_B                 0x400000bdu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_B) */
#define KEY_KP_C                 0x400000beu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_C) */
#define KEY_KP_D                 0x400000bfu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_D) */
#define KEY_KP_E                 0x400000c0u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_E) */
#define KEY_KP_F                 0x400000c1u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_F) */
#define KEY_KP_XOR               0x400000c2u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_XOR) */
#define KEY_KP_POWER             0x400000c3u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_POWER) */
#define KEY_KP_PERCENT           0x400000c4u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_PERCENT) */
#define KEY_KP_LESS              0x400000c5u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_LESS) */
#define KEY_KP_GREATER           0x400000c6u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_GREATER) */
#define KEY_KP_AMPERSAND         0x400000c7u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_AMPERSAND) */
#define KEY_KP_DBLAMPERSAND      0x400000c8u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_DBLAMPERSAND) */
#define KEY_KP_VERTICALBAR       0x400000c9u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_VERTICALBAR) */
#define KEY_KP_DBLVERTICALBAR    0x400000cau /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_DBLVERTICALBAR) */
#define KEY_KP_COLON             0x400000cbu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_COLON) */
#define KEY_KP_HASH              0x400000ccu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_HASH) */
#define KEY_KP_SPACE             0x400000cdu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_SPACE) */
#define KEY_KP_AT                0x400000ceu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_AT) */
#define KEY_KP_EXCLAM            0x400000cfu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_EXCLAM) */
#define KEY_KP_MEMSTORE          0x400000d0u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMSTORE) */
#define KEY_KP_MEMRECALL         0x400000d1u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMRECALL) */
#define KEY_KP_MEMCLEAR          0x400000d2u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMCLEAR) */
#define KEY_KP_MEMADD            0x400000d3u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMADD) */
#define KEY_KP_MEMSUBTRACT       0x400000d4u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMSUBTRACT) */
#define KEY_KP_MEMMULTIPLY       0x400000d5u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMMULTIPLY) */
#define KEY_KP_MEMDIVIDE         0x400000d6u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMDIVIDE) */
#define KEY_KP_PLUSMINUS         0x400000d7u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_PLUSMINUS) */
#define KEY_KP_CLEAR             0x400000d8u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_CLEAR) */
#define KEY_KP_CLEARENTRY        0x400000d9u /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_CLEARENTRY) */
#define KEY_KP_BINARY            0x400000dau /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_BINARY) */
#define KEY_KP_OCTAL             0x400000dbu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_OCTAL) */
#define KEY_KP_DECIMAL           0x400000dcu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_DECIMAL) */
#define KEY_KP_HEXADECIMAL       0x400000ddu /**< SCANCODE_TO_KEYCODE(SCANCODE_KP_HEXADECIMAL) */
#define KEY_LCTRL                0x400000e0u /**< SCANCODE_TO_KEYCODE(SCANCODE_LCTRL) */
#define KEY_LSHIFT               0x400000e1u /**< SCANCODE_TO_KEYCODE(SCANCODE_LSHIFT) */
#define KEY_LALT                 0x400000e2u /**< SCANCODE_TO_KEYCODE(SCANCODE_LALT) */
#define KEY_LGUI                 0x400000e3u /**< SCANCODE_TO_KEYCODE(SCANCODE_LGUI) */
#define KEY_RCTRL                0x400000e4u /**< SCANCODE_TO_KEYCODE(SCANCODE_RCTRL) */
#define KEY_RSHIFT               0x400000e5u /**< SCANCODE_TO_KEYCODE(SCANCODE_RSHIFT) */
#define KEY_RALT                 0x400000e6u /**< SCANCODE_TO_KEYCODE(SCANCODE_RALT) */
#define KEY_RGUI                 0x400000e7u /**< SCANCODE_TO_KEYCODE(SCANCODE_RGUI) */
#define KEY_MODE                 0x40000101u /**< SCANCODE_TO_KEYCODE(SCANCODE_MODE) */
#define KEY_SLEEP                0x40000102u /**< SCANCODE_TO_KEYCODE(SCANCODE_SLEEP) */
#define KEY_WAKE                 0x40000103u /**< SCANCODE_TO_KEYCODE(SCANCODE_WAKE) */
#define KEY_CHANNEL_INCREMENT    0x40000104u /**< SCANCODE_TO_KEYCODE(SCANCODE_CHANNEL_INCREMENT) */
#define KEY_CHANNEL_DECREMENT    0x40000105u /**< SCANCODE_TO_KEYCODE(SCANCODE_CHANNEL_DECREMENT) */
#define KEY_MEDIA_PLAY           0x40000106u /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PLAY) */
#define KEY_MEDIA_PAUSE          0x40000107u /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PAUSE) */
#define KEY_MEDIA_RECORD         0x40000108u /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_RECORD) */
#define KEY_MEDIA_FAST_FORWARD   0x40000109u /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_FAST_FORWARD) */
#define KEY_MEDIA_REWIND         0x4000010au /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_REWIND) */
#define KEY_MEDIA_NEXT_TRACK     0x4000010bu /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_NEXT_TRACK) */
#define KEY_MEDIA_PREVIOUS_TRACK 0x4000010cu /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PREVIOUS_TRACK) */
#define KEY_MEDIA_STOP           0x4000010du /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_STOP) */
#define KEY_MEDIA_EJECT          0x4000010eu /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_EJECT) */
#define KEY_MEDIA_PLAY_PAUSE     0x4000010fu /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PLAY_PAUSE) */
#define KEY_MEDIA_SELECT         0x40000110u /**< SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_SELECT) */
#define KEY_AC_NEW               0x40000111u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_NEW) */
#define KEY_AC_OPEN              0x40000112u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_OPEN) */
#define KEY_AC_CLOSE             0x40000113u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_CLOSE) */
#define KEY_AC_EXIT              0x40000114u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_EXIT) */
#define KEY_AC_SAVE              0x40000115u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_SAVE) */
#define KEY_AC_PRINT             0x40000116u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_PRINT) */
#define KEY_AC_PROPERTIES        0x40000117u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_PROPERTIES) */
#define KEY_AC_SEARCH            0x40000118u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_SEARCH) */
#define KEY_AC_HOME              0x40000119u /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_HOME) */
#define KEY_AC_BACK              0x4000011au /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_BACK) */
#define KEY_AC_FORWARD           0x4000011bu /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_FORWARD) */
#define KEY_AC_STOP              0x4000011cu /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_STOP) */
#define KEY_AC_REFRESH           0x4000011du /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_REFRESH) */
#define KEY_AC_BOOKMARKS         0x4000011eu /**< SCANCODE_TO_KEYCODE(SCANCODE_AC_BOOKMARKS) */
#define KEY_SOFTLEFT             0x4000011fu /**< SCANCODE_TO_KEYCODE(SCANCODE_SOFTLEFT) */
#define KEY_SOFTRIGHT            0x40000120u /**< SCANCODE_TO_KEYCODE(SCANCODE_SOFTRIGHT) */
#define KEY_CALL                 0x40000121u /**< SCANCODE_TO_KEYCODE(SCANCODE_CALL) */
#define KEY_ENDCALL              0x40000122u /**< SCANCODE_TO_KEYCODE(SCANCODE_ENDCALL) */
#define KEY_FN                   SCANCODE_TO_KEYCODE(KEY_SCANCODE_FN)
#define KEY_SQUARE               SCANCODE_TO_KEYCODE(KEY_SCANCODE_SQUARE)
#define KEY_TRIANGLE             SCANCODE_TO_KEYCODE(KEY_SCANCODE_TRIANGLE)
#define KEY_CROSS                SCANCODE_TO_KEYCODE(KEY_SCANCODE_CROSS)
#define KEY_CIRCLE               SCANCODE_TO_KEYCODE(KEY_SCANCODE_CIRCLE)
#define KEY_CLOUD                SCANCODE_TO_KEYCODE(KEY_SCANCODE_CLOUD)
#define KEY_DIAMOND              SCANCODE_TO_KEYCODE(KEY_SCANCODE_DIAMOND)
#define KEY_LEFT_TAB             0x20000001u /**< Extended key Left Tab */
#define KEY_LEVEL5_SHIFT         0x20000002u /**< Extended key Level 5 Shift */
#define KEY_MULTI_KEY_COMPOSE    0x20000003u /**< Extended key Multi-key Compose */
#define KEY_LMETA                0x20000004u /**< Extended key Left Meta */
#define KEY_RMETA                0x20000005u /**< Extended key Right Meta */
#define KEY_LHYPER               0x20000006u /**< Extended key Left Hyper */
#define KEY_RHYPER               0x20000007u /**< Extended key Right Hyper */

#define KMOD_NONE   0x0000u  /**< no modifier is applicable. */
#define KMOD_LSHIFT 0x0001u  /**< the left Shift key is down. */
#define KMOD_RSHIFT 0x0002u  /**< the right Shift key is down. */
#define KMOD_LEVEL5 0x0004u  /**< the Level 5 Shift key is down. */
#define KMOD_LCTRL  0x0040u  /**< the left Ctrl (Control) key is down. */
#define KMOD_RCTRL  0x0080u  /**< the right Ctrl (Control) key is down. */
#define KMOD_LALT   0x0100u  /**< the left Alt key is down. */
#define KMOD_RALT   0x0200u  /**< the right Alt key is down. */
#define KMOD_LGUI   0x0400u  /**< the left GUI key (often the Windows key) is down. */
#define KMOD_RGUI   0x0800u  /**< the right GUI key (often the Windows key) is down. */
#define KMOD_NUM    0x1000u  /**< the Num Lock key (may be located on an extended keypad) is down. */
#define KMOD_CAPS   0x2000u  /**< the Caps Lock key is down. */
#define KMOD_MODE   0x4000u  /**< the !AltGr key is down. */
#define KMOD_SCROLL 0x8000u  /**< the Scroll Lock key is down. */
#define KMOD_FN     0x10000u /**< the FN key is down. */
#define KMOD_CTRL   (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)   /**< Any Ctrl key is down. */
#define KMOD_SHIFT  (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT) /**< Any Shift key is down. */
#define KMOD_ALT    (SDL_KMOD_LALT | SDL_KMOD_RALT)     /**< Any Alt key is down. */
#define KMOD_GUI    (SDL_KMOD_LGUI | SDL_KMOD_RGUI)     /**< Any GUI key is down. */

// From SDL3
typedef struct {
    event_type_t        type;      /**< EVENT_KEY_DOWN or EVENT_KEY_UP */
    uint64_t            timestamp; /**< In nanoseconds, populated using gettimeofday */
    keyboard_scancode_t scancode;  /**< SDL physical key code */
    key_code_t          key;       /**< SDL virtual key code */
    key_mod_t           mod;       /**< current key modifiers */
    bool                down;      /**< true if the key is pressed */
    bool                repeat;    /**< true if this is a key repeat */
} keyboard_event_t;

typedef struct {
    uint64_t            timestamp; /**< In nanoseconds, populated using gettimeofday */
    keyboard_scancode_t scancode;  /**< SDL physical key code */
    bool                down;      /**< true if the key is pressed */
} keyboard_event_ll_t;


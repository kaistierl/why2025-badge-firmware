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

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t key_code_t;
typedef uint16_t key_mod_t;

typedef struct {
    char unshifted;
    char shifted;
} keyboard_ascii_value_t;

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


#define BADGEVMS_KEY_EXTENDED_MASK      (1u << 29)
#define BADGEVMS_KEY_SCANCODE_MASK      (1u << 30)
#define BADGEVMS_SCANCODE_TO_KEYCODE(X) (X | BADGEVMS_KEY_SCANCODE_MASK)
#define BADGEVMS_KEY_UNKNOWN            0x00000000u /**< 0 */
#define BADGEVMS_KEY_RETURN             0x0000000du /**< '\r' */
#define BADGEVMS_KEY_ESCAPE             0x0000001bu /**< '\x1B' */
#define BADGEVMS_KEY_BACKSPACE          0x00000008u /**< '\b' */
#define BADGEVMS_KEY_TAB                0x00000009u /**< '\t' */
#define BADGEVMS_KEY_SPACE              0x00000020u /**< ' ' */
#define BADGEVMS_KEY_EXCLAIM            0x00000021u /**< '!' */
#define BADGEVMS_KEY_DBLAPOSTROPHE      0x00000022u /**< '"' */
#define BADGEVMS_KEY_HASH               0x00000023u /**< '#' */
#define BADGEVMS_KEY_DOLLAR             0x00000024u /**< '$' */
#define BADGEVMS_KEY_PERCENT            0x00000025u /**< '%' */
#define BADGEVMS_KEY_AMPERSAND          0x00000026u /**< '&' */
#define BADGEVMS_KEY_APOSTROPHE         0x00000027u /**< '\'' */
#define BADGEVMS_KEY_LEFTPAREN          0x00000028u /**< '(' */
#define BADGEVMS_KEY_RIGHTPAREN         0x00000029u /**< ')' */
#define BADGEVMS_KEY_ASTERISK           0x0000002au /**< '*' */
#define BADGEVMS_KEY_PLUS               0x0000002bu /**< '+' */
#define BADGEVMS_KEY_COMMA              0x0000002cu /**< ',' */
#define BADGEVMS_KEY_MINUS              0x0000002du /**< '-' */
#define BADGEVMS_KEY_PERIOD             0x0000002eu /**< '.' */
#define BADGEVMS_KEY_SLASH              0x0000002fu /**< '/' */
#define BADGEVMS_KEY_0                  0x00000030u /**< '0' */
#define BADGEVMS_KEY_1                  0x00000031u /**< '1' */
#define BADGEVMS_KEY_2                  0x00000032u /**< '2' */
#define BADGEVMS_KEY_3                  0x00000033u /**< '3' */
#define BADGEVMS_KEY_4                  0x00000034u /**< '4' */
#define BADGEVMS_KEY_5                  0x00000035u /**< '5' */
#define BADGEVMS_KEY_6                  0x00000036u /**< '6' */
#define BADGEVMS_KEY_7                  0x00000037u /**< '7' */
#define BADGEVMS_KEY_8                  0x00000038u /**< '8' */
#define BADGEVMS_KEY_9                  0x00000039u /**< '9' */
#define BADGEVMS_KEY_COLON              0x0000003au /**< ':' */
#define BADGEVMS_KEY_SEMICOLON          0x0000003bu /**< ';' */
#define BADGEVMS_KEY_LESS               0x0000003cu /**< '<' */
#define BADGEVMS_KEY_EQUALS             0x0000003du /**< '=' */
#define BADGEVMS_KEY_GREATER            0x0000003eu /**< '>' */
#define BADGEVMS_KEY_QUESTION           0x0000003fu /**< '?' */
#define BADGEVMS_KEY_AT                 0x00000040u /**< '@' */
#define BADGEVMS_KEY_LEFTBRACKET        0x0000005bu /**< '[' */
#define BADGEVMS_KEY_BACKSLASH          0x0000005cu /**< '\\' */
#define BADGEVMS_KEY_RIGHTBRACKET       0x0000005du /**< ']' */
#define BADGEVMS_KEY_CARET              0x0000005eu /**< '^' */
#define BADGEVMS_KEY_UNDERSCORE         0x0000005fu /**< '_' */
#define BADGEVMS_KEY_GRAVE              0x00000060u /**< '`' */
#define BADGEVMS_KEY_A                  0x00000061u /**< 'a' */
#define BADGEVMS_KEY_B                  0x00000062u /**< 'b' */
#define BADGEVMS_KEY_C                  0x00000063u /**< 'c' */
#define BADGEVMS_KEY_D                  0x00000064u /**< 'd' */
#define BADGEVMS_KEY_E                  0x00000065u /**< 'e' */
#define BADGEVMS_KEY_F                  0x00000066u /**< 'f' */
#define BADGEVMS_KEY_G                  0x00000067u /**< 'g' */
#define BADGEVMS_KEY_H                  0x00000068u /**< 'h' */
#define BADGEVMS_KEY_I                  0x00000069u /**< 'i' */
#define BADGEVMS_KEY_J                  0x0000006au /**< 'j' */
#define BADGEVMS_KEY_K                  0x0000006bu /**< 'k' */
#define BADGEVMS_KEY_L                  0x0000006cu /**< 'l' */
#define BADGEVMS_KEY_M                  0x0000006du /**< 'm' */
#define BADGEVMS_KEY_N                  0x0000006eu /**< 'n' */
#define BADGEVMS_KEY_O                  0x0000006fu /**< 'o' */
#define BADGEVMS_KEY_P                  0x00000070u /**< 'p' */
#define BADGEVMS_KEY_Q                  0x00000071u /**< 'q' */
#define BADGEVMS_KEY_R                  0x00000072u /**< 'r' */
#define BADGEVMS_KEY_S                  0x00000073u /**< 's' */
#define BADGEVMS_KEY_T                  0x00000074u /**< 't' */
#define BADGEVMS_KEY_U                  0x00000075u /**< 'u' */
#define BADGEVMS_KEY_V                  0x00000076u /**< 'v' */
#define BADGEVMS_KEY_W                  0x00000077u /**< 'w' */
#define BADGEVMS_KEY_X                  0x00000078u /**< 'x' */
#define BADGEVMS_KEY_Y                  0x00000079u /**< 'y' */
#define BADGEVMS_KEY_Z                  0x0000007au /**< 'z' */
#define BADGEVMS_KEY_LEFTBRACE          0x0000007bu /**< '{' */
#define BADGEVMS_KEY_PIPE               0x0000007cu /**< '|' */
#define BADGEVMS_KEY_RIGHTBRACE         0x0000007du /**< '}' */
#define BADGEVMS_KEY_TILDE              0x0000007eu /**< '~' */
#define BADGEVMS_KEY_DELETE             0x0000007fu /**< '\x7F' */
#define BADGEVMS_KEY_PLUSMINUS          0x000000b1u /**< '\xB1' */
#define BADGEVMS_KEY_CAPSLOCK           0x40000039u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CAPSLOCK) */
#define BADGEVMS_KEY_F1                 0x4000003au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F1) */
#define BADGEVMS_KEY_F2                 0x4000003bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F2) */
#define BADGEVMS_KEY_F3                 0x4000003cu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F3) */
#define BADGEVMS_KEY_F4                 0x4000003du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F4) */
#define BADGEVMS_KEY_F5                 0x4000003eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F5) */
#define BADGEVMS_KEY_F6                 0x4000003fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F6) */
#define BADGEVMS_KEY_F7                 0x40000040u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F7) */
#define BADGEVMS_KEY_F8                 0x40000041u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F8) */
#define BADGEVMS_KEY_F9                 0x40000042u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F9) */
#define BADGEVMS_KEY_F10                0x40000043u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F10) */
#define BADGEVMS_KEY_F11                0x40000044u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F11) */
#define BADGEVMS_KEY_F12                0x40000045u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F12) */
#define BADGEVMS_KEY_PRINTSCREEN        0x40000046u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_PRINTSCREEN) */
#define BADGEVMS_KEY_SCROLLLOCK         0x40000047u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SCROLLLOCK) */
#define BADGEVMS_KEY_PAUSE              0x40000048u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_PAUSE) */
#define BADGEVMS_KEY_INSERT             0x40000049u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_INSERT) */
#define BADGEVMS_KEY_HOME               0x4000004au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_HOME) */
#define BADGEVMS_KEY_PAGEUP             0x4000004bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_PAGEUP) */
#define BADGEVMS_KEY_END                0x4000004du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_END) */
#define BADGEVMS_KEY_PAGEDOWN           0x4000004eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_PAGEDOWN) */
#define BADGEVMS_KEY_RIGHT              0x4000004fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_RIGHT) */
#define BADGEVMS_KEY_LEFT               0x40000050u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_LEFT) */
#define BADGEVMS_KEY_DOWN               0x40000051u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_DOWN) */
#define BADGEVMS_KEY_UP                 0x40000052u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_UP) */
#define BADGEVMS_KEY_NUMLOCKCLEAR       0x40000053u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_NUMLOCKCLEAR) */
#define BADGEVMS_KEY_KP_DIVIDE          0x40000054u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_DIVIDE) */
#define BADGEVMS_KEY_KP_MULTIPLY        0x40000055u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MULTIPLY) */
#define BADGEVMS_KEY_KP_MINUS           0x40000056u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MINUS) */
#define BADGEVMS_KEY_KP_PLUS            0x40000057u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_PLUS) */
#define BADGEVMS_KEY_KP_ENTER           0x40000058u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_ENTER) */
#define BADGEVMS_KEY_KP_1               0x40000059u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_1) */
#define BADGEVMS_KEY_KP_2               0x4000005au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_2) */
#define BADGEVMS_KEY_KP_3               0x4000005bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_3) */
#define BADGEVMS_KEY_KP_4               0x4000005cu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_4) */
#define BADGEVMS_KEY_KP_5               0x4000005du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_5) */
#define BADGEVMS_KEY_KP_6               0x4000005eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_6) */
#define BADGEVMS_KEY_KP_7               0x4000005fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_7) */
#define BADGEVMS_KEY_KP_8               0x40000060u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_8) */
#define BADGEVMS_KEY_KP_9               0x40000061u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_9) */
#define BADGEVMS_KEY_KP_0               0x40000062u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_0) */
#define BADGEVMS_KEY_KP_PERIOD          0x40000063u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_PERIOD) */
#define BADGEVMS_KEY_APPLICATION        0x40000065u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_APPLICATION) */
#define BADGEVMS_KEY_POWER              0x40000066u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_POWER) */
#define BADGEVMS_KEY_KP_EQUALS          0x40000067u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_EQUALS) */
#define BADGEVMS_KEY_F13                0x40000068u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F13) */
#define BADGEVMS_KEY_F14                0x40000069u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F14) */
#define BADGEVMS_KEY_F15                0x4000006au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F15) */
#define BADGEVMS_KEY_F16                0x4000006bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F16) */
#define BADGEVMS_KEY_F17                0x4000006cu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F17) */
#define BADGEVMS_KEY_F18                0x4000006du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F18) */
#define BADGEVMS_KEY_F19                0x4000006eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F19) */
#define BADGEVMS_KEY_F20                0x4000006fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F20) */
#define BADGEVMS_KEY_F21                0x40000070u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F21) */
#define BADGEVMS_KEY_F22                0x40000071u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F22) */
#define BADGEVMS_KEY_F23                0x40000072u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F23) */
#define BADGEVMS_KEY_F24                0x40000073u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_F24) */
#define BADGEVMS_KEY_EXECUTE            0x40000074u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_EXECUTE) */
#define BADGEVMS_KEY_HELP               0x40000075u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_HELP) */
#define BADGEVMS_KEY_MENU               0x40000076u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MENU) */
#define BADGEVMS_KEY_SELECT             0x40000077u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SELECT) */
#define BADGEVMS_KEY_STOP               0x40000078u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_STOP) */
#define BADGEVMS_KEY_AGAIN              0x40000079u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AGAIN) */
#define BADGEVMS_KEY_UNDO               0x4000007au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_UNDO) */
#define BADGEVMS_KEY_CUT                0x4000007bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CUT) */
#define BADGEVMS_KEY_COPY               0x4000007cu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_COPY) */
#define BADGEVMS_KEY_PASTE              0x4000007du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_PASTE) */
#define BADGEVMS_KEY_FIND               0x4000007eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_FIND) */
#define BADGEVMS_KEY_MUTE               0x4000007fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MUTE) */
#define BADGEVMS_KEY_VOLUMEUP           0x40000080u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_VOLUMEUP) */
#define BADGEVMS_KEY_VOLUMEDOWN         0x40000081u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_VOLUMEDOWN) */
#define BADGEVMS_KEY_KP_COMMA           0x40000085u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_COMMA) */
#define BADGEVMS_KEY_KP_EQUALSAS400     0x40000086u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_EQUALSAS400) */
#define BADGEVMS_KEY_ALTERASE           0x40000099u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_ALTERASE) */
#define BADGEVMS_KEY_SYSREQ             0x4000009au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SYSREQ) */
#define BADGEVMS_KEY_CANCEL             0x4000009bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CANCEL) */
#define BADGEVMS_KEY_CLEAR              0x4000009cu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CLEAR) */
#define BADGEVMS_KEY_PRIOR              0x4000009du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_PRIOR) */
#define BADGEVMS_KEY_RETURN2            0x4000009eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_RETURN2) */
#define BADGEVMS_KEY_SEPARATOR          0x4000009fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SEPARATOR) */
#define BADGEVMS_KEY_OUT                0x400000a0u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_OUT) */
#define BADGEVMS_KEY_OPER               0x400000a1u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_OPER) */
#define BADGEVMS_KEY_CLEARAGAIN         0x400000a2u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CLEARAGAIN) */
#define BADGEVMS_KEY_CRSEL              0x400000a3u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CRSEL) */
#define BADGEVMS_KEY_EXSEL              0x400000a4u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_EXSEL) */
#define BADGEVMS_KEY_KP_00              0x400000b0u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_00) */
#define BADGEVMS_KEY_KP_000             0x400000b1u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_000) */
#define BADGEVMS_KEY_THOUSANDSSEPARATOR 0x400000b2u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_THOUSANDSSEPARATOR) */
#define BADGEVMS_KEY_DECIMALSEPARATOR   0x400000b3u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_DECIMALSEPARATOR) */
#define BADGEVMS_KEY_CURRENCYUNIT       0x400000b4u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CURRENCYUNIT) */
#define BADGEVMS_KEY_CURRENCYSUBUNIT    0x400000b5u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CURRENCYSUBUNIT) */
#define BADGEVMS_KEY_KP_LEFTPAREN       0x400000b6u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_LEFTPAREN) */
#define BADGEVMS_KEY_KP_RIGHTPAREN      0x400000b7u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_RIGHTPAREN) */
#define BADGEVMS_KEY_KP_LEFTBRACE       0x400000b8u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_LEFTBRACE) */
#define BADGEVMS_KEY_KP_RIGHTBRACE      0x400000b9u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_RIGHTBRACE) */
#define BADGEVMS_KEY_KP_TAB             0x400000bau /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_TAB) */
#define BADGEVMS_KEY_KP_BACKSPACE       0x400000bbu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_BACKSPACE) */
#define BADGEVMS_KEY_KP_A               0x400000bcu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_A) */
#define BADGEVMS_KEY_KP_B               0x400000bdu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_B) */
#define BADGEVMS_KEY_KP_C               0x400000beu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_C) */
#define BADGEVMS_KEY_KP_D               0x400000bfu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_D) */
#define BADGEVMS_KEY_KP_E               0x400000c0u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_E) */
#define BADGEVMS_KEY_KP_F               0x400000c1u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_F) */
#define BADGEVMS_KEY_KP_XOR             0x400000c2u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_XOR) */
#define BADGEVMS_KEY_KP_POWER           0x400000c3u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_POWER) */
#define BADGEVMS_KEY_KP_PERCENT         0x400000c4u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_PERCENT) */
#define BADGEVMS_KEY_KP_LESS            0x400000c5u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_LESS) */
#define BADGEVMS_KEY_KP_GREATER         0x400000c6u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_GREATER) */
#define BADGEVMS_KEY_KP_AMPERSAND       0x400000c7u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_AMPERSAND) */
#define BADGEVMS_KEY_KP_DBLAMPERSAND    0x400000c8u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_DBLAMPERSAND) */
#define BADGEVMS_KEY_KP_VERTICALBAR     0x400000c9u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_VERTICALBAR) */
#define BADGEVMS_KEY_KP_DBLVERTICALBAR  0x400000cau /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_DBLVERTICALBAR) */
#define BADGEVMS_KEY_KP_COLON           0x400000cbu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_COLON) */
#define BADGEVMS_KEY_KP_HASH            0x400000ccu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_HASH) */
#define BADGEVMS_KEY_KP_SPACE           0x400000cdu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_SPACE) */
#define BADGEVMS_KEY_KP_AT              0x400000ceu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_AT) */
#define BADGEVMS_KEY_KP_EXCLAM          0x400000cfu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_EXCLAM) */
#define BADGEVMS_KEY_KP_MEMSTORE        0x400000d0u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMSTORE) */
#define BADGEVMS_KEY_KP_MEMRECALL       0x400000d1u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMRECALL) */
#define BADGEVMS_KEY_KP_MEMCLEAR        0x400000d2u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMCLEAR) */
#define BADGEVMS_KEY_KP_MEMADD          0x400000d3u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMADD) */
#define BADGEVMS_KEY_KP_MEMSUBTRACT     0x400000d4u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMSUBTRACT) */
#define BADGEVMS_KEY_KP_MEMMULTIPLY     0x400000d5u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMMULTIPLY) */
#define BADGEVMS_KEY_KP_MEMDIVIDE       0x400000d6u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_MEMDIVIDE) */
#define BADGEVMS_KEY_KP_PLUSMINUS       0x400000d7u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_PLUSMINUS) */
#define BADGEVMS_KEY_KP_CLEAR           0x400000d8u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_CLEAR) */
#define BADGEVMS_KEY_KP_CLEARENTRY      0x400000d9u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_CLEARENTRY) */
#define BADGEVMS_KEY_KP_BINARY          0x400000dau /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_BINARY) */
#define BADGEVMS_KEY_KP_OCTAL           0x400000dbu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_OCTAL) */
#define BADGEVMS_KEY_KP_DECIMAL         0x400000dcu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_DECIMAL) */
#define BADGEVMS_KEY_KP_HEXADECIMAL     0x400000ddu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_KP_HEXADECIMAL) */
#define BADGEVMS_KEY_LCTRL              0x400000e0u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_LCTRL) */
#define BADGEVMS_KEY_LSHIFT             0x400000e1u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_LSHIFT) */
#define BADGEVMS_KEY_LALT               0x400000e2u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_LALT) */
#define BADGEVMS_KEY_LGUI               0x400000e3u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_LGUI) */
#define BADGEVMS_KEY_RCTRL              0x400000e4u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_RCTRL) */
#define BADGEVMS_KEY_RSHIFT             0x400000e5u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_RSHIFT) */
#define BADGEVMS_KEY_RALT               0x400000e6u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_RALT) */
#define BADGEVMS_KEY_RGUI               0x400000e7u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_RGUI) */
#define BADGEVMS_KEY_MODE               0x40000101u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MODE) */
#define BADGEVMS_KEY_SLEEP              0x40000102u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SLEEP) */
#define BADGEVMS_KEY_WAKE               0x40000103u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_WAKE) */
#define BADGEVMS_KEY_CHANNEL_INCREMENT  0x40000104u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CHANNEL_INCREMENT) */
#define BADGEVMS_KEY_CHANNEL_DECREMENT  0x40000105u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CHANNEL_DECREMENT) */
#define BADGEVMS_KEY_MEDIA_PLAY         0x40000106u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PLAY) */
#define BADGEVMS_KEY_MEDIA_PAUSE        0x40000107u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PAUSE) */
#define BADGEVMS_KEY_MEDIA_RECORD       0x40000108u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_RECORD) */
#define BADGEVMS_KEY_MEDIA_FAST_FORWARD 0x40000109u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_FAST_FORWARD) */
#define BADGEVMS_KEY_MEDIA_REWIND       0x4000010au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_REWIND) */
#define BADGEVMS_KEY_MEDIA_NEXT_TRACK   0x4000010bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_NEXT_TRACK) */
#define BADGEVMS_KEY_MEDIA_PREVIOUS_TRACK                                                                              \
    0x4000010cu                                    /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PREVIOUS_TRACK) */
#define BADGEVMS_KEY_MEDIA_STOP        0x4000010du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_STOP) */
#define BADGEVMS_KEY_MEDIA_EJECT       0x4000010eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_EJECT) */
#define BADGEVMS_KEY_MEDIA_PLAY_PAUSE  0x4000010fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_PLAY_PAUSE) */
#define BADGEVMS_KEY_MEDIA_SELECT      0x40000110u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_MEDIA_SELECT) */
#define BADGEVMS_KEY_AC_NEW            0x40000111u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_NEW) */
#define BADGEVMS_KEY_AC_OPEN           0x40000112u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_OPEN) */
#define BADGEVMS_KEY_AC_CLOSE          0x40000113u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_CLOSE) */
#define BADGEVMS_KEY_AC_EXIT           0x40000114u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_EXIT) */
#define BADGEVMS_KEY_AC_SAVE           0x40000115u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_SAVE) */
#define BADGEVMS_KEY_AC_PRINT          0x40000116u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_PRINT) */
#define BADGEVMS_KEY_AC_PROPERTIES     0x40000117u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_PROPERTIES) */
#define BADGEVMS_KEY_AC_SEARCH         0x40000118u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_SEARCH) */
#define BADGEVMS_KEY_AC_HOME           0x40000119u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_HOME) */
#define BADGEVMS_KEY_AC_BACK           0x4000011au /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_BACK) */
#define BADGEVMS_KEY_AC_FORWARD        0x4000011bu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_FORWARD) */
#define BADGEVMS_KEY_AC_STOP           0x4000011cu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_STOP) */
#define BADGEVMS_KEY_AC_REFRESH        0x4000011du /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_REFRESH) */
#define BADGEVMS_KEY_AC_BOOKMARKS      0x4000011eu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_AC_BOOKMARKS) */
#define BADGEVMS_KEY_SOFTLEFT          0x4000011fu /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SOFTLEFT) */
#define BADGEVMS_KEY_SOFTRIGHT         0x40000120u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_SOFTRIGHT) */
#define BADGEVMS_KEY_CALL              0x40000121u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_CALL) */
#define BADGEVMS_KEY_ENDCALL           0x40000122u /**< BADGEVMS_SCANCODE_TO_KEYCODE(SCANCODE_ENDCALL) */
#define BADGEVMS_KEY_FN                BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_FN)
#define BADGEVMS_KEY_SQUARE            BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_SQUARE)
#define BADGEVMS_KEY_TRIANGLE          BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_TRIANGLE)
#define BADGEVMS_KEY_CROSS             BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_CROSS)
#define BADGEVMS_KEY_CIRCLE            BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_CIRCLE)
#define BADGEVMS_KEY_CLOUD             BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_CLOUD)
#define BADGEVMS_KEY_DIAMOND           BADGEVMS_SCANCODE_TO_KEYCODE(KEY_SCANCODE_DIAMOND)
#define BADGEVMS_KEY_LEFT_TAB          0x20000001u /**< Extended key Left Tab */
#define BADGEVMS_KEY_LEVEL5_SHIFT      0x20000002u /**< Extended key Level 5 Shift */
#define BADGEVMS_KEY_MULTI_KEY_COMPOSE 0x20000003u /**< Extended key Multi-key Compose */
#define BADGEVMS_KEY_LMETA             0x20000004u /**< Extended key Left Meta */
#define BADGEVMS_KEY_RMETA             0x20000005u /**< Extended key Right Meta */
#define BADGEVMS_KEY_LHYPER            0x20000006u /**< Extended key Left Hyper */
#define BADGEVMS_KEY_RHYPER            0x20000007u /**< Extended key Right Hyper */

#define BADGEVMS_KMOD_NONE   0x0000u /**< no modifier is applicable. */
#define BADGEVMS_KMOD_LSHIFT 0x0001u /**< the left Shift key is down. */
#define BADGEVMS_KMOD_RSHIFT 0x0002u /**< the right Shift key is down. */
#define BADGEVMS_KMOD_LEVEL5 0x0004u /**< the Level 5 Shift key is down. */
#define BADGEVMS_KMOD_LCTRL  0x0040u /**< the left Ctrl (Control) key is down. */
#define BADGEVMS_KMOD_RCTRL  0x0080u /**< the right Ctrl (Control) key is down. */
#define BADGEVMS_KMOD_LALT   0x0100u /**< the left Alt key is down. */
#define BADGEVMS_KMOD_RALT   0x0200u /**< the right Alt key is down. */
#define BADGEVMS_KMOD_LGUI   0x0400u /**< the left GUI key (often the Windows key) is down. */
#define BADGEVMS_KMOD_RGUI   0x0800u /**< the right GUI key (often the Windows key) is down. */
#define BADGEVMS_KMOD_NUM    0x1000u /**< the Num Lock key (may be located on an extended keypad) is down. */
#define BADGEVMS_KMOD_CAPS   0x2000u /**< the Caps Lock key is down. */
#define BADGEVMS_KMOD_MODE   0x4000u /**< the !AltGr key is down. */
#define BADGEVMS_KMOD_SCROLL 0x8000u /**< the Scroll Lock key is down. */
#define BADGEVMS_KMOD_CTRL   (BADGEVMS_KMOD_LCTRL | BADGEVMS_KMOD_RCTRL)   /**< Any Ctrl key is down. */
#define BADGEVMS_KMOD_SHIFT  (BADGEVMS_KMOD_LSHIFT | BADGEVMS_KMOD_RSHIFT) /**< Any Shift key is down. */
#define BADGEVMS_KMOD_ALT    (BADGEVMS_KMOD_LALT | BADGEVMS_KMOD_RALT)     /**< Any Alt key is down. */
#define BADGEVMS_KMOD_GUI    (BADGEVMS_KMOD_LGUI | BADGEVMS_KMOD_RGUI)     /**< Any GUI key is down. */

#include "keymap_us.h"

static inline char keyboard_get_ascii(keyboard_scancode_t scancode, key_mod_t modifiers) {
    if (scancode >= KEY_SCANCODE_COUNT) {
        return 0;
    }

    keyboard_ascii_value_t const *mapping = &keyboard_map[scancode];

    if (modifiers & (BADGEVMS_KMOD_LSHIFT | BADGEVMS_KMOD_RSHIFT)) {
        return mapping->shifted;
    } else {
        return mapping->unshifted;
    }
}

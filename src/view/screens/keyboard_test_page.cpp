/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "keyboard_test_page.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

#include "asset_manager.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

namespace screen {
namespace {

using KeyLayer = KeyboardTestPage::KeyLayer;

enum class KeyFont {
  TEXT,
  KENNEY,
  EXTRA,
};

struct KeySpec {
  KeyLayer layer;
  const char* match;
  const char* text;
  int32_t row;
  int32_t col;
  KeyFont font;
  bool disabled{false};
};

struct LayerMetrics {
  int32_t width;
  int32_t height;
  int32_t stride_x;
  int32_t stride_y;
  int32_t layout_width;
  int32_t layout_height;
};

constexpr int32_t K_KEY_SIZE      = 32;
constexpr int32_t K_KEY_STRIDE    = 26;
constexpr int32_t K_ROW_STRIDE    = 20;
constexpr int32_t K_LAYOUT_WIDTH  = K_KEY_SIZE + 10 * K_KEY_STRIDE;
constexpr int32_t K_LAYOUT_HEIGHT = K_KEY_SIZE + 4 * K_ROW_STRIDE;

constexpr const char* K_MATCH_0               = "0";
constexpr const char* K_MATCH_1               = "1";
constexpr const char* K_MATCH_2               = "2";
constexpr const char* K_MATCH_3               = "3";
constexpr const char* K_MATCH_4               = "4";
constexpr const char* K_MATCH_5               = "5";
constexpr const char* K_MATCH_6               = "6";
constexpr const char* K_MATCH_7               = "7";
constexpr const char* K_MATCH_8               = "8";
constexpr const char* K_MATCH_9               = "9";
constexpr const char* K_MATCH_A               = "a";
constexpr const char* K_MATCH_ALT             = "alt";
constexpr const char* K_MATCH_B               = "b";
constexpr const char* K_MATCH_BACKSPACE       = "backspace";
constexpr const char* K_MATCH_BRIGHTNESS_DOWN = "brightnessdown";
constexpr const char* K_MATCH_BRIGHTNESS_UP   = "brightnessup";
constexpr const char* K_MATCH_C               = "c";
constexpr const char* K_MATCH_CTRL            = "ctrl";
constexpr const char* K_MATCH_D               = "d";
constexpr const char* K_MATCH_DELETE          = "delete";
constexpr const char* K_MATCH_DISABLED        = "__disabled";
constexpr const char* K_MATCH_DOWN            = "down";
constexpr const char* K_MATCH_E               = "e";
constexpr const char* K_MATCH_END             = "end";
constexpr const char* K_MATCH_ENTER           = "enter";
constexpr const char* K_MATCH_ESC             = "esc";
constexpr const char* K_MATCH_F               = "f";
constexpr const char* K_MATCH_F1              = "f1";
constexpr const char* K_MATCH_F2              = "f2";
constexpr const char* K_MATCH_F3              = "f3";
constexpr const char* K_MATCH_F4              = "f4";
constexpr const char* K_MATCH_F5              = "f5";
constexpr const char* K_MATCH_F6              = "f6";
constexpr const char* K_MATCH_F7              = "f7";
constexpr const char* K_MATCH_F8              = "f8";
constexpr const char* K_MATCH_F9              = "f9";
constexpr const char* K_MATCH_F10             = "f10";
constexpr const char* K_MATCH_F11             = "f11";
constexpr const char* K_MATCH_F12             = "f12";
constexpr const char* K_MATCH_FAST_FORWARD    = "fastforward";
constexpr const char* K_MATCH_FN              = "fn";
constexpr const char* K_MATCH_G               = "g";
constexpr const char* K_MATCH_H               = "h";
constexpr const char* K_MATCH_HOME            = "home";
constexpr const char* K_MATCH_I               = "i";
constexpr const char* K_MATCH_INSERT          = "insert";
constexpr const char* K_MATCH_J               = "j";
constexpr const char* K_MATCH_K               = "k";
constexpr const char* K_MATCH_L               = "l";
constexpr const char* K_MATCH_LEFT            = "left";
constexpr const char* K_MATCH_M               = "m";
constexpr const char* K_MATCH_N               = "n";
constexpr const char* K_MATCH_NEXT            = "next";
constexpr const char* K_MATCH_O               = "o";
constexpr const char* K_MATCH_P               = "p";
constexpr const char* K_MATCH_PAGE_DOWN       = "pagedown";
constexpr const char* K_MATCH_PAGE_UP         = "pageup";
constexpr const char* K_MATCH_PLAY_PAUSE      = "playpause";
constexpr const char* K_MATCH_PRINTSCREEN     = "printscreen";
constexpr const char* K_MATCH_Q               = "q";
constexpr const char* K_MATCH_R               = "r";
constexpr const char* K_MATCH_REWIND          = "rewind";
constexpr const char* K_MATCH_RIGHT           = "right";
constexpr const char* K_MATCH_S               = "s";
constexpr const char* K_MATCH_SHIFT           = "shift";
constexpr const char* K_MATCH_SPACE           = "space";
constexpr const char* K_MATCH_SYM             = "sym";
constexpr const char* K_MATCH_T               = "t";
constexpr const char* K_MATCH_TAB             = "tab";
constexpr const char* K_MATCH_U               = "u";
constexpr const char* K_MATCH_UP              = "up";
constexpr const char* K_MATCH_V               = "v";
constexpr const char* K_MATCH_VOLUME_DOWN     = "volumedown";
constexpr const char* K_MATCH_VOLUME_MUTE     = "volumemute";
constexpr const char* K_MATCH_VOLUME_UP       = "volumeup";
constexpr const char* K_MATCH_W               = "w";
constexpr const char* K_MATCH_X               = "x";
constexpr const char* K_MATCH_Y               = "y";
constexpr const char* K_MATCH_Z               = "z";

constexpr const char* ICON_KB_APOSTROPHE         = "\uE01B";
constexpr const char* ICON_KB_ARROW_DOWN         = "\uE01D";
constexpr const char* ICON_KB_ARROW_LEFT         = "\uE01F";
constexpr const char* ICON_KB_ARROW_RIGHT        = "\uE021";
constexpr const char* ICON_KB_ARROW_UP           = "\uE023";
constexpr const char* ICON_KB_ASTERISK           = "\uE034";
constexpr const char* ICON_KB_BACKSPACE_ICON_ALT = "\uE03A";
constexpr const char* ICON_KB_BRACKET_R          = "\uE03E";
constexpr const char* ICON_KB_GREATER            = "\uE040";
constexpr const char* ICON_KB_LESS               = "\uE042";
constexpr const char* ICON_KB_BRACKET_L          = "\uE044";
constexpr const char* ICON_KB_CARET              = "\uE04C";
constexpr const char* ICON_KB_COLON              = "\uE04E";
constexpr const char* ICON_KB_COMMA              = "\uE050";
constexpr const char* ICON_KB_DELETE             = "\uE058";
constexpr const char* ICON_KB_END                = "\uE05C";
constexpr const char* ICON_KB_EQUALS             = "\uE060";
constexpr const char* ICON_KB_EXCLAMATION        = "\uE064";
constexpr const char* ICON_KB_F1                 = "\uE067";
constexpr const char* ICON_KB_F10                = "\uE068";
constexpr const char* ICON_KB_F11                = "\uE06A";
constexpr const char* ICON_KB_F12                = "\uE06C";
constexpr const char* ICON_KB_F2                 = "\uE06F";
constexpr const char* ICON_KB_F3                 = "\uE071";
constexpr const char* ICON_KB_F4                 = "\uE073";
constexpr const char* ICON_KB_F5                 = "\uE075";
constexpr const char* ICON_KB_F6                 = "\uE077";
constexpr const char* ICON_KB_F7                 = "\uE079";
constexpr const char* ICON_KB_F8                 = "\uE07B";
constexpr const char* ICON_KB_F9                 = "\uE07D";
constexpr const char* ICON_KB_HOME               = "\uE086";
constexpr const char* ICON_KB_INSERT             = "\uE08A";
constexpr const char* ICON_KB_MINUS              = "\uE094";
constexpr const char* ICON_KB_NUMPAD_ENTER       = "\uE09A";
constexpr const char* ICON_KB_PAGE_DOWN          = "\uE0A5";
constexpr const char* ICON_KB_PAGE_UP            = "\uE0A7";
constexpr const char* ICON_KB_PERIOD             = "\uE0AD";
constexpr const char* ICON_KB_PLUS               = "\uE0AF";
constexpr const char* ICON_KB_PRINTSCREEN        = "\uE0B1";
constexpr const char* ICON_KB_QUESTION           = "\uE0B5";
constexpr const char* ICON_KB_QUOTE              = "\uE0B7";
constexpr const char* ICON_KB_SEMICOLON          = "\uE0C1";
constexpr const char* ICON_KB_SHIFT_ICON         = "\uE0C4";
constexpr const char* ICON_KB_SLASH_BACK         = "\uE0C7";
constexpr const char* ICON_KB_SLASH              = "\uE0C9";
constexpr const char* ICON_KB_SPACE_ICON         = "\uE0CC";
constexpr const char* ICON_KB_TILDE              = "\uE0D7";
constexpr const char* ICON_KB_UNDERSCORE         = "\uE0DB";

constexpr const char* ICON_KB_EXTRA_AMPERSAND    = "\uEA01";
constexpr const char* ICON_KB_EXTRA_AT_SIGN      = "\uEA03";
constexpr const char* ICON_KB_EXTRA_BACKTICK     = "\uEA05";
constexpr const char* ICON_KB_EXTRA_BRACE_R      = "\uEA09";
constexpr const char* ICON_KB_EXTRA_BRACE_L      = "\uEA0B";
constexpr const char* ICON_KB_EXTRA_DOLLAR       = "\uEA0D";
constexpr const char* ICON_KB_EXTRA_HASH         = "\uEA0F";
constexpr const char* ICON_KB_EXTRA_PAREN_R      = "\uEA11";
constexpr const char* ICON_KB_EXTRA_PAREN_L      = "\uEA13";
constexpr const char* ICON_KB_EXTRA_PERCENT      = "\uEA15";
constexpr const char* ICON_KB_EXTRA_PIPE         = "\uEA17";
constexpr const char* ICON_KB_EXTRA_BLANK        = "\uEA07";
constexpr const char* ICON_KB_EXTRA_SYM          = "\uEA23";
constexpr const char* ICON_KB_EXTRA_SPEAKER_HIGH = "\uEA19";
constexpr const char* ICON_KB_EXTRA_SPEAKER_LOW  = "\uEA1B";
constexpr const char* ICON_KB_EXTRA_SPEAKER_X    = "\uEA1D";
constexpr const char* ICON_KB_EXTRA_SUN          = "\uEA1F";
constexpr const char* ICON_KB_EXTRA_SUN_DIM      = "\uEA20";

constexpr const char* ICON_KB_EXTRA_PLAY_PAUSE   = "\uEA25";
constexpr const char* ICON_KB_EXTRA_REWIND       = "\uEA26";
constexpr const char* ICON_KB_EXTRA_FAST_FORWARD = "\uEA27";
constexpr const char* ICON_KB_EXTRA_QUESTION     = "\uEA28";

constexpr KeyLayer STANDARD_KEY = KeyLayer::STANDARD_KEY;
constexpr KeyLayer FN_KEY       = KeyLayer::FN_KEY;
constexpr KeyLayer SYM_KEY      = KeyLayer::SYM_KEY;

constexpr std::array<KeySpec, 138> K_KEYS = {{
    {STANDARD_KEY, K_MATCH_ESC, view::ICON_KB_ESC, 0, 0, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_TAB, view::ICON_KB_TAB, 0, 10, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_1, view::ICON_KB_1, 1, 0, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_2, view::ICON_KB_2, 1, 1, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_3, view::ICON_KB_3, 1, 2, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_4, view::ICON_KB_4, 1, 3, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_5, view::ICON_KB_5, 1, 4, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_6, view::ICON_KB_6, 1, 5, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_7, view::ICON_KB_7, 1, 6, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_8, view::ICON_KB_8, 1, 7, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_9, view::ICON_KB_9, 1, 8, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_0, view::ICON_KB_0, 1, 9, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_BACKSPACE, ICON_KB_BACKSPACE_ICON_ALT, 1, 10, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_SYM, ICON_KB_EXTRA_SYM, 2, 0, KeyFont::EXTRA},
    {STANDARD_KEY, K_MATCH_Q, view::ICON_KB_Q, 2, 1, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_W, view::ICON_KB_W, 2, 2, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_E, view::ICON_KB_E, 2, 3, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_R, view::ICON_KB_R, 2, 4, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_T, view::ICON_KB_T, 2, 5, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_Y, view::ICON_KB_Y, 2, 6, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_U, view::ICON_KB_U, 2, 7, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_I, view::ICON_KB_I, 2, 8, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_O, view::ICON_KB_O, 2, 9, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_P, view::ICON_KB_P, 2, 10, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_SHIFT, ICON_KB_SHIFT_ICON, 3, 0, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_A, view::ICON_KB_A, 3, 1, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_S, view::ICON_KB_S, 3, 2, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_D, view::ICON_KB_D, 3, 3, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_F, view::ICON_KB_F, 3, 4, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_G, view::ICON_KB_G, 3, 5, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_H, view::ICON_KB_H, 3, 6, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_J, view::ICON_KB_J, 3, 7, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_K, view::ICON_KB_K, 3, 8, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_L, view::ICON_KB_L, 3, 9, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_ENTER, ICON_KB_NUMPAD_ENTER, 3, 10, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_FN, view::ICON_KB_FN, 4, 0, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_CTRL, view::ICON_KB_CTRL, 4, 1, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_ALT, view::ICON_KB_ALT, 4, 2, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_Z, view::ICON_KB_Z, 4, 3, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_X, view::ICON_KB_X, 4, 4, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_C, view::ICON_KB_C, 4, 5, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_V, view::ICON_KB_V, 4, 6, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_B, view::ICON_KB_B, 4, 7, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_N, view::ICON_KB_N, 4, 8, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_M, view::ICON_KB_M, 4, 9, KeyFont::KENNEY},
    {STANDARD_KEY, K_MATCH_SPACE, ICON_KB_SPACE_ICON, 4, 10, KeyFont::KENNEY},

    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 0, 0, KeyFont::EXTRA, true},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 0, 10, KeyFont::EXTRA, true},

    {FN_KEY, K_MATCH_F1, ICON_KB_F1, 1, 0, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F2, ICON_KB_F2, 1, 1, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F3, ICON_KB_F3, 1, 2, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F4, ICON_KB_F4, 1, 3, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F5, ICON_KB_F5, 1, 4, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F6, ICON_KB_F6, 1, 5, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F7, ICON_KB_F7, 1, 6, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F8, ICON_KB_F8, 1, 7, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F9, ICON_KB_F9, 1, 8, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F10, ICON_KB_F10, 1, 9, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_DELETE, ICON_KB_DELETE, 1, 10, KeyFont::KENNEY},

    {FN_KEY, K_MATCH_SYM, ICON_KB_EXTRA_SYM, 2, 0, KeyFont::EXTRA, true},
    {FN_KEY, K_MATCH_PLAY_PAUSE, ICON_KB_EXTRA_PLAY_PAUSE, 2, 1, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_REWIND, ICON_KB_EXTRA_REWIND, 2, 2, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_FAST_FORWARD, ICON_KB_EXTRA_FAST_FORWARD, 2, 3, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 2, 4, KeyFont::EXTRA, true},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 2, 5, KeyFont::EXTRA, true},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 2, 6, KeyFont::EXTRA, true},
    {FN_KEY, K_MATCH_BRIGHTNESS_DOWN, ICON_KB_EXTRA_SUN_DIM, 2, 7, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_BRIGHTNESS_UP, ICON_KB_EXTRA_SUN, 2, 8, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_F11, ICON_KB_F11, 2, 9, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_F12, ICON_KB_F12, 2, 10, KeyFont::KENNEY},

    {FN_KEY, K_MATCH_SHIFT, ICON_KB_SHIFT_ICON, 3, 0, KeyFont::KENNEY, true},
    {FN_KEY, K_MATCH_VOLUME_MUTE, ICON_KB_EXTRA_SPEAKER_X, 3, 1, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_VOLUME_DOWN, ICON_KB_EXTRA_SPEAKER_LOW, 3, 2, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_VOLUME_UP, ICON_KB_EXTRA_SPEAKER_HIGH, 3, 3, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_UP, ICON_KB_ARROW_UP, 3, 4, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 3, 5, KeyFont::EXTRA, true},
    {FN_KEY, "?", ICON_KB_EXTRA_QUESTION, 3, 6, KeyFont::EXTRA},
    {FN_KEY, K_MATCH_PRINTSCREEN, ICON_KB_PRINTSCREEN, 3, 7, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_HOME, ICON_KB_HOME, 3, 8, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_PAGE_UP, ICON_KB_PAGE_UP, 3, 9, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 3, 10, KeyFont::EXTRA, true},

    {FN_KEY, K_MATCH_FN, view::ICON_KB_FN, 4, 0, KeyFont::KENNEY, true},
    {FN_KEY, K_MATCH_CTRL, view::ICON_KB_CTRL, 4, 1, KeyFont::KENNEY, true},
    {FN_KEY, K_MATCH_ALT, view::ICON_KB_ALT, 4, 2, KeyFont::KENNEY, true},
    {FN_KEY, K_MATCH_LEFT, ICON_KB_ARROW_LEFT, 4, 3, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_DOWN, ICON_KB_ARROW_DOWN, 4, 4, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_RIGHT, ICON_KB_ARROW_RIGHT, 4, 5, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 4, 6, KeyFont::EXTRA, true},
    {FN_KEY, K_MATCH_INSERT, ICON_KB_INSERT, 4, 7, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_END, ICON_KB_END, 4, 8, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_PAGE_DOWN, ICON_KB_PAGE_DOWN, 4, 9, KeyFont::KENNEY},
    {FN_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 4, 10, KeyFont::EXTRA, true},

    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 0, 0, KeyFont::EXTRA, true},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 0, 10, KeyFont::EXTRA, true},

    {SYM_KEY, "!", ICON_KB_EXCLAMATION, 1, 0, KeyFont::KENNEY},
    {SYM_KEY, "@", ICON_KB_EXTRA_AT_SIGN, 1, 1, KeyFont::EXTRA},
    {SYM_KEY, "#", ICON_KB_EXTRA_HASH, 1, 2, KeyFont::EXTRA},
    {SYM_KEY, "$", ICON_KB_EXTRA_DOLLAR, 1, 3, KeyFont::EXTRA},
    {SYM_KEY, "%", ICON_KB_EXTRA_PERCENT, 1, 4, KeyFont::EXTRA},
    {SYM_KEY, "^", ICON_KB_CARET, 1, 5, KeyFont::KENNEY},
    {SYM_KEY, "&", ICON_KB_EXTRA_AMPERSAND, 1, 6, KeyFont::EXTRA},
    {SYM_KEY, "*", ICON_KB_ASTERISK, 1, 7, KeyFont::KENNEY},
    {SYM_KEY, "(", ICON_KB_EXTRA_PAREN_L, 1, 8, KeyFont::EXTRA},
    {SYM_KEY, ")", ICON_KB_EXTRA_PAREN_R, 1, 9, KeyFont::EXTRA},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 1, 10, KeyFont::EXTRA, true},

    {SYM_KEY, K_MATCH_SYM, ICON_KB_EXTRA_SYM, 2, 0, KeyFont::EXTRA, true},
    {SYM_KEY, "~", ICON_KB_TILDE, 2, 1, KeyFont::KENNEY},
    {SYM_KEY, "`", ICON_KB_EXTRA_BACKTICK, 2, 2, KeyFont::EXTRA},
    {SYM_KEY, "+", ICON_KB_PLUS, 2, 3, KeyFont::KENNEY},
    {SYM_KEY, "-", ICON_KB_MINUS, 2, 4, KeyFont::KENNEY},
    {SYM_KEY, "/", ICON_KB_SLASH, 2, 5, KeyFont::KENNEY},
    {SYM_KEY, "\\", ICON_KB_SLASH_BACK, 2, 6, KeyFont::KENNEY},
    {SYM_KEY, "{", ICON_KB_EXTRA_BRACE_L, 2, 7, KeyFont::EXTRA},
    {SYM_KEY, "}", ICON_KB_EXTRA_BRACE_R, 2, 8, KeyFont::EXTRA},
    {SYM_KEY, "[", ICON_KB_BRACKET_L, 2, 9, KeyFont::KENNEY},
    {SYM_KEY, "]", ICON_KB_BRACKET_R, 2, 10, KeyFont::KENNEY},

    {SYM_KEY, K_MATCH_SHIFT, ICON_KB_SHIFT_ICON, 3, 0, KeyFont::KENNEY, true},
    {SYM_KEY, ",", ICON_KB_COMMA, 3, 1, KeyFont::KENNEY},
    {SYM_KEY, ".", ICON_KB_PERIOD, 3, 2, KeyFont::KENNEY},
    {SYM_KEY, "|", ICON_KB_EXTRA_PIPE, 3, 3, KeyFont::EXTRA},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 3, 4, KeyFont::EXTRA, true},
    {SYM_KEY, "=", ICON_KB_EQUALS, 3, 5, KeyFont::KENNEY},
    {SYM_KEY, ":", ICON_KB_COLON, 3, 6, KeyFont::KENNEY},
    {SYM_KEY, ";", ICON_KB_SEMICOLON, 3, 7, KeyFont::KENNEY},
    {SYM_KEY, "_", ICON_KB_UNDERSCORE, 3, 8, KeyFont::KENNEY},
    {SYM_KEY, "?", ICON_KB_QUESTION, 3, 9, KeyFont::KENNEY},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 3, 10, KeyFont::EXTRA, true},

    {SYM_KEY, K_MATCH_FN, view::ICON_KB_FN, 4, 0, KeyFont::KENNEY, true},
    {SYM_KEY, K_MATCH_CTRL, view::ICON_KB_CTRL, 4, 1, KeyFont::KENNEY, true},
    {SYM_KEY, K_MATCH_ALT, view::ICON_KB_ALT, 4, 2, KeyFont::KENNEY, true},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 4, 3, KeyFont::EXTRA, true},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 4, 4, KeyFont::EXTRA, true},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 4, 5, KeyFont::EXTRA, true},
    {SYM_KEY, "<", ICON_KB_LESS, 4, 6, KeyFont::KENNEY},
    {SYM_KEY, ">", ICON_KB_GREATER, 4, 7, KeyFont::KENNEY},
    {SYM_KEY, "'", ICON_KB_APOSTROPHE, 4, 8, KeyFont::KENNEY},
    {SYM_KEY, "\"", ICON_KB_QUOTE, 4, 9, KeyFont::KENNEY},
    {SYM_KEY, K_MATCH_DISABLED, ICON_KB_EXTRA_BLANK, 4, 10, KeyFont::EXTRA, true},
}};

std::size_t layer_index(KeyLayer layer) {
  switch (layer) {
    case FN_KEY:
      return 1;
    case SYM_KEY:
      return 2;
    case STANDARD_KEY:
    default:
      return 0;
  }
}

bool is_counted_key(const KeySpec& spec) {
  if (spec.disabled) {
    return false;
  }

  if (spec.layer != STANDARD_KEY) {
    return true;
  }

  const std::string_view match = spec.match ? spec.match : "";
  return match != K_MATCH_FN && match != K_MATCH_SHIFT && match != K_MATCH_SYM;
}

std::string normalize_key_name(uint32_t key, const char* key_name) {
  std::string normalized = key_name ? key_name : "";
  if (normalized.empty()) {
    if (key == ' ') {
      normalized = K_MATCH_SPACE;
    } else if (key == '\t') {
      normalized = K_MATCH_TAB;
    } else if (key >= 32 && key <= 126) {
      normalized = static_cast<char>(key);
    }
  }

  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  normalized.erase(std::remove(normalized.begin(), normalized.end(), ' '), normalized.end());
  return normalized;
}

bool matches_key(const KeySpec& spec, const std::string& normalized_key, uint32_t key) {
  if (spec.disabled) {
    return false;
  }

  const std::string_view match = spec.match ? spec.match : "";
  if (match == K_MATCH_TAB) {
    return normalized_key == K_MATCH_NEXT || normalized_key == K_MATCH_TAB || key == LV_KEY_NEXT ||
           key == '\t';
  }
  if (match == K_MATCH_SPACE) {
    return normalized_key == K_MATCH_SPACE || key == ' ';
  }
  if (match == K_MATCH_VOLUME_MUTE) {
    return normalized_key == K_MATCH_VOLUME_MUTE || normalized_key == "mute";
  }
  return normalized_key == match;
}

const char* layer_title(KeyLayer layer) {
  switch (layer) {
    case FN_KEY:
      return "FN_KEY";
    case SYM_KEY:
      return "SYM_KEY";
    case STANDARD_KEY:
    default:
      return "STANDARD_KEY";
  }
}

KeyLayer next_layer(KeyLayer layer) {
  switch (layer) {
    case STANDARD_KEY:
      return FN_KEY;
    case FN_KEY:
      return SYM_KEY;
    case SYM_KEY:
    default:
      return SYM_KEY;
  }
}

KeyLayer next_layer_cyclic(KeyLayer layer) {
  switch (layer) {
    case STANDARD_KEY:
      return FN_KEY;
    case FN_KEY:
      return SYM_KEY;
    case SYM_KEY:
    default:
      return STANDARD_KEY;
  }
}

bool is_tab_input(uint32_t key, const char* key_name) {
  const auto normalized_key = normalize_key_name(key, key_name);
  return normalized_key == K_MATCH_NEXT || normalized_key == K_MATCH_TAB || key == LV_KEY_NEXT ||
         key == '\t';
}

LayerMetrics metrics_for_layer(KeyLayer layer) {
  switch (layer) {
    case FN_KEY:
      return {K_KEY_SIZE, K_KEY_SIZE, K_KEY_STRIDE, K_ROW_STRIDE, K_LAYOUT_WIDTH, K_LAYOUT_HEIGHT};
    case SYM_KEY:
      return {K_KEY_SIZE, K_KEY_SIZE, K_KEY_STRIDE, K_ROW_STRIDE, K_LAYOUT_WIDTH, K_LAYOUT_HEIGHT};
    case STANDARD_KEY:
    default:
      return {K_KEY_SIZE, K_KEY_SIZE, K_KEY_STRIDE, K_ROW_STRIDE, K_LAYOUT_WIDTH, K_LAYOUT_HEIGHT};
  }
}

lv_dir_t tile_dirs(std::size_t index) {
  switch (index) {
    case 0:
      return LV_DIR_RIGHT;
    case 1:
      return LV_DIR_HOR;
    case 2:
    default:
      return LV_DIR_LEFT;
  }
}

lv_color_t pressed_color_for_layer(KeyLayer layer, bool dark_mode) {
  const auto colors = view::palette(dark_mode);
  switch (layer) {
    case FN_KEY:
      return colors.warning_pressed;
    case SYM_KEY:
      return colors.info;
    case STANDARD_KEY:
    default:
      return colors.success;
  }
}

}  // namespace

KeyboardTestPage::KeyboardTestPage(viewmodel::AppViewModel& app_view_model,
                                   viewmodel::KeyboardTestViewModel& keyboard_view_model,
                                   app::AssetManager& assets)
    : BaseScreen(app_view_model, assets),
      keyboard_view_model_(keyboard_view_model) {
  platform::set_nav_trigger_mode(platform::NavTriggerMode::LONG_PRESS);
  update_nav_actions_();
  init();
  platform::set_key_listener(key_listener, this);
  platform::set_key_release_listener(key_release_listener, this);
  platform::set_long_key_listener(long_key_listener, this);
}

KeyboardTestPage::~KeyboardTestPage() {
  platform::clear_long_key_listener(long_key_listener, this);
  platform::clear_key_release_listener(key_release_listener, this);
  platform::clear_key_listener(key_listener, this);
  hold_popup_.reset();
}

void KeyboardTestPage::build_content(lv_obj_t* content) {
  if (auto* title_bar = title_bar_ref_()) {
    title_bar->set_title_alignment(LV_ALIGN_LEFT_MID, 8, 0);
  }

  key_buttons_.resize(K_KEYS.size());
  key_states_.assign(K_KEYS.size(), KeyState::IDLE);
  layer_tiles_.assign(K_LAYER_COUNT, nullptr);
  layer_built_.fill(false);
  layer_pressed_counts_.fill(0);
  layer_total_counts_.fill(0);
  for (const auto& key : K_KEYS) {
    if (is_counted_key(key)) {
      ++layer_total_counts_[layer_index(key.layer)];
    }
  }
  active_layer_ = KeyLayer::STANDARD_KEY;
  update_progress_();

  tileview_ = lv_tileview_create(content);
  lv_obj_set_size(tileview_, LV_PCT(100), LV_PCT(100));
  lv_obj_center(tileview_);
  lv_obj_set_scrollbar_mode(tileview_, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(tileview_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tileview_, 0, 0);
  lv_obj_set_style_pad_all(tileview_, 0, 0);
  lv_obj_add_event_cb(tileview_, tile_scroll_end_cb, LV_EVENT_SCROLL_END, this);

  key_font_       = assets_ref_().load_font("kenney_input_keyboard_and_mouse.ttf", 30);
  extra_key_font_ = assets_ref_().load_font("kenny_keyboard_extra.ttf", 30);

  for (std::size_t layer = 0; layer < layer_tiles_.size(); ++layer) {
    auto* tile = lv_tileview_add_tile(tileview_, static_cast<uint8_t>(layer), 0, tile_dirs(layer));
    lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    layer_tiles_[layer] = tile;
  }

  build_layer_(KeyLayer::STANDARD_KEY);
  transition_to_(KeyLayer::STANDARD_KEY, false);
}

void KeyboardTestPage::update_progress_() {
  char buffer[40];
  const auto index = layer_index(active_layer_);
  std::snprintf(buffer,
                sizeof(buffer),
                "%s %zu/%zu",
                app_view_model_ref_().tr(layer_title(active_layer_)).c_str(),
                layer_pressed_counts_[index],
                layer_total_counts_[index]);
  if (auto* title_bar = title_bar_ref_()) {
    title_bar->set_center_text(buffer);
  }
}

void KeyboardTestPage::update_button_state_(std::size_t index) {
  if (index >= key_buttons_.size() || !key_buttons_[index]) {
    return;
  }

  switch (key_states_[index]) {
    case KeyState::PRESSED:
      key_buttons_[index]->set_color(pressed_color_for_layer(K_KEYS[index].layer, false),
                                     pressed_color_for_layer(K_KEYS[index].layer, true));
      break;
    case KeyState::FAILED:
      key_buttons_[index]->set_color(view::palette(false).error, view::palette(true).error);
      break;
    case KeyState::IDLE:
    default:
      key_buttons_[index]->set_color(view::palette(false).text_muted,
                                     view::palette(true).text_muted);
      break;
  }
}

void KeyboardTestPage::build_layer_(KeyLayer layer) {
  const auto layer_idx = layer_index(layer);
  if (layer_idx >= layer_built_.size() || layer_built_[layer_idx]) {
    return;
  }

  auto* tile = layer_idx < layer_tiles_.size() ? layer_tiles_[layer_idx] : nullptr;
  if (!tile) {
    return;
  }

  const auto metrics = metrics_for_layer(layer);
  auto* layout       = lv_obj_create(tile);
  lv_obj_remove_style_all(layout);
  lv_obj_set_size(layout, metrics.layout_width, metrics.layout_height);
  lv_obj_clear_flag(layout, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(layout, LV_ALIGN_TOP_MID, 0, 0);

  for (std::size_t i = 0; i < K_KEYS.size(); ++i) {
    const auto& spec = K_KEYS[i];
    if (spec.layer != layer) {
      continue;
    }

    const lv_font_t* font = &lv_font_montserrat_12;
    switch (spec.font) {
      case KeyFont::EXTRA:
        font = extra_key_font_ ? extra_key_font_ : (key_font_ ? key_font_ : &lv_font_montserrat_14);
        break;
      case KeyFont::KENNEY:
        font = key_font_ ? key_font_ : &lv_font_montserrat_14;
        break;
      case KeyFont::TEXT:
      default:
        break;
    }

    key_buttons_[i] = std::make_unique<view::widgets::IconButton>(layout,
                                                                  app_view_model_ref_(),
                                                                  metrics.width,
                                                                  metrics.height,
                                                                  spec.text,
                                                                  font,
                                                                  view::palette(false).text_muted,
                                                                  view::palette(true).text_muted);
    key_buttons_[i]->build();
    if (key_buttons_[i]->root()) {
      lv_obj_clear_flag(key_buttons_[i]->root(), LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_pos(key_buttons_[i]->root(),
                     spec.col * metrics.stride_x,
                     spec.row * metrics.stride_y);
    }
    update_button_state_(i);
    if (spec.disabled) {
      key_buttons_[i]->set_enabled(false);
    }
  }

  layer_built_[layer_idx] = true;
}

void KeyboardTestPage::transition_to_(KeyLayer layer, bool animate) {
  active_layer_ = layer;
  update_progress_();
  update_nav_actions_();
  build_layer_(layer);

  const auto index = layer_index(layer);
  if (!tileview_ || index >= layer_tiles_.size() || !layer_tiles_[index]) {
    return;
  }

  lv_tileview_set_tile(tileview_, layer_tiles_[index], animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void KeyboardTestPage::sync_active_tile_() {
  if (!tileview_) {
    return;
  }

  auto* active_tile = lv_tileview_get_tile_active(tileview_);
  for (std::size_t i = 0; i < layer_tiles_.size(); ++i) {
    if (active_tile == layer_tiles_[i]) {
      active_layer_ = static_cast<KeyLayer>(i);
      build_layer_(active_layer_);
      update_progress_();
      update_nav_actions_();
      return;
    }
  }
}

void KeyboardTestPage::update_nav_actions_() {
  app_view_model_ref_().clear_nav_actions();
  set_nav_action_(
      '4',
      view::ICON_ARROW_U_UP_LEFT,
      [this]() {
        hide_hold_popup_();
        app_view_model_ref_().request_back_or_quit();
      },
      LV_EVENT_LONG_PRESSED,
      viewmodel::NavHoldTarget::ERROR,
      false,
      [this]() { show_hold_popup_("Hold 4 to exit", view::widgets::PopupTone::ERROR); },
      [this]() { hide_hold_popup_(); });

  if (active_layer_ == KeyLayer::SYM_KEY) {
    set_nav_action_(
        '8',
        view::ICON_CHECK_SQUARE,
        [this]() {
          hide_hold_popup_();
          show_test_result_dialog_();
        },
        LV_EVENT_LONG_PRESSED,
        viewmodel::NavHoldTarget::SUCCESS,
        false,
        [this]() {
          show_hold_popup_("Hold 8 to confirm test", view::widgets::PopupTone::SUCCESS);
        },
        [this]() { hide_hold_popup_(); });
  }
}

void KeyboardTestPage::switch_to_next_layout_() {
  transition_to_(next_layer_cyclic(active_layer_));
}

void KeyboardTestPage::show_hold_popup_(const char* message, view::widgets::PopupTone tone) {
  if (!root()) {
    return;
  }
  if (hold_popup_ && hold_popup_tone_ != tone) {
    hold_popup_.reset();
  }
  if (!hold_popup_) {
    view::widgets::PopupConfig config;
    config.width       = 250;
    config.label_width = 234;
    config.message     = app_view_model_ref_().tr(message ? message : "");
    config.tone        = tone;
    config.font        = assets_ref_().load_font(app_view_model_ref_().ui_font_name("inter-semibold.ttf"), 12);
    hold_popup_tone_   = tone;
    hold_popup_ = std::make_unique<view::widgets::Popup>(root(), app_view_model_ref_(), config);
    hold_popup_->build();
  } else {
    const auto text = app_view_model_ref_().tr(message ? message : "");
    hold_popup_->set_text(text.c_str());
  }
  hold_popup_->show();
}

void KeyboardTestPage::hide_hold_popup_() {
  if (hold_popup_) {
    hold_popup_->hide();
  }
}

void KeyboardTestPage::mark_key_pressed_(uint32_t key, const char* key_name) {
  const auto normalized_key = normalize_key_name(key, key_name);
  for (std::size_t i = 0; i < K_KEYS.size(); ++i) {
    if (!matches_key(K_KEYS[i], normalized_key, key)) {
      continue;
    }

    const bool counted = is_counted_key(K_KEYS[i]);
    if (key_states_[i] != KeyState::PRESSED) {
      key_states_[i] = KeyState::PRESSED;
      if (counted) {
        ++layer_pressed_counts_[layer_index(K_KEYS[i].layer)];
      }
      update_button_state_(i);
    }

    const auto index = layer_index(K_KEYS[i].layer);
    if (counted && K_KEYS[i].layer == active_layer_ &&
        layer_pressed_counts_[index] >= layer_total_counts_[index]) {
      transition_to_(next_layer(K_KEYS[i].layer));
    } else {
      update_progress_();
    }
    return;
  }
}

void KeyboardTestPage::key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<KeyboardTestPage*>(user_data);
  if (!page) {
    return;
  }
  if (page->handle_test_result_dialog_key_(key, key_name)) {
    return;
  }

  if (is_tab_input(key, key_name)) {
    page->show_hold_popup_("Hold Tab to switch layout", view::widgets::PopupTone::WARNING);
  }

  page->keyboard_view_model_.record_key(key_name ? key_name : "");
  page->mark_key_pressed_(key, key_name);
}

void KeyboardTestPage::key_release_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<KeyboardTestPage*>(user_data);
  if (page && is_tab_input(key, key_name)) {
    page->hide_hold_popup_();
  }
}

void KeyboardTestPage::long_key_listener(uint32_t key, const char* key_name, void* user_data) {
  auto* page = static_cast<KeyboardTestPage*>(user_data);
  if (!page || !is_tab_input(key, key_name)) {
    return;
  }

  page->hide_hold_popup_();
  page->switch_to_next_layout_();
}

void KeyboardTestPage::tile_scroll_end_cb(lv_event_t* event) {
  auto* page = static_cast<KeyboardTestPage*>(lv_event_get_user_data(event));
  if (page) {
    page->sync_active_tile_();
  }
}

}  // namespace screen

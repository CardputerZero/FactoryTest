/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <cstdint>
namespace view {

// some font constants
constexpr const char* ICON_SIGN_OUT           = "\uE42A";
constexpr const char* ICON_ARROW_U_UP_LEFT    = "\uE08A";
constexpr const char* ICON_TEXT_BOLD          = "\uE5BE";
constexpr const char* ICON_MOON               = "\uE330";
constexpr const char* ICON_SUN                = "\uE474";
constexpr const char* ICON_SQUARE_ARROW_LEFT  = "\uE074";
constexpr const char* ICON_SQUARE_ARROW_RIGHT = "\uE076";
constexpr const char* ICON_MINUS              = "\uE32A";
constexpr const char* ICON_PLUS               = "\uE3D4";
constexpr const char* ICON_INFO               = "\uE2CE";
constexpr const char* ICON_CHECK_SQUARE       = "\uE186";
constexpr const char* ICON_CLOCK              = "\uE19A";
constexpr const char* ICON_X_SQUARE           = "\uE4F6";
constexpr const char* ICON_FLASK              = "\uE79E";
constexpr const char* ICON_KEYBOARD           = "\uE2D8";
constexpr const char* ICON_KB_0               = "\uE001";
constexpr const char* ICON_KB_1               = "\uE003";
constexpr const char* ICON_KB_2               = "\uE005";
constexpr const char* ICON_KB_3               = "\uE007";
constexpr const char* ICON_KB_4               = "\uE009";
constexpr const char* ICON_KB_5               = "\uE00B";
constexpr const char* ICON_KB_6               = "\uE00D";
constexpr const char* ICON_KB_7               = "\uE00F";
constexpr const char* ICON_KB_8               = "\uE011";
constexpr const char* ICON_KB_9               = "\uE013";
constexpr const char* ICON_KB_A               = "\uE015";
constexpr const char* ICON_KB_ALT             = "\uE017";
constexpr const char* ICON_KB_ANY             = "\uE019";
constexpr const char* ICON_KB_B               = "\uE036";
constexpr const char* ICON_KB_BACKSPACE       = "\uE038";
constexpr const char* ICON_KB_C               = "\uE046";
constexpr const char* ICON_KB_CTRL            = "\uE054";
constexpr const char* ICON_KB_D               = "\uE056";
constexpr const char* ICON_KB_E               = "\uE05A";
constexpr const char* ICON_KB_ENTER           = "\uE05E";
constexpr const char* ICON_KB_ESC             = "\uE062";
constexpr const char* ICON_KB_F               = "\uE066";
constexpr const char* ICON_KB_FN              = "\uE080";
constexpr const char* ICON_KB_G               = "\uE082";
constexpr const char* ICON_KB_H               = "\uE084";
constexpr const char* ICON_KB_I               = "\uE088";
constexpr const char* ICON_KB_J               = "\uE08C";
constexpr const char* ICON_KB_K               = "\uE08E";
constexpr const char* ICON_KB_L               = "\uE090";
constexpr const char* ICON_KB_M               = "\uE092";
constexpr const char* ICON_KB_N               = "\uE096";
constexpr const char* ICON_KB_O               = "\uE09E";
constexpr const char* ICON_KB_P               = "\uE0A3";
constexpr const char* ICON_KB_Q               = "\uE0B3";
constexpr const char* ICON_KB_R               = "\uE0B9";
constexpr const char* ICON_KB_S               = "\uE0BD";
constexpr const char* ICON_KB_SHIFT           = "\uE0C3";
constexpr const char* ICON_KB_SPACE           = "\uE0CB";
constexpr const char* ICON_KB_T               = "\uE0CF";
constexpr const char* ICON_KB_TAB             = "\uE0D1";
constexpr const char* ICON_KB_U               = "\uE0D9";
constexpr const char* ICON_KB_V               = "\uE0DD";
constexpr const char* ICON_KB_W               = "\uE0DF";
constexpr const char* ICON_KB_X               = "\uE0E3";
constexpr const char* ICON_KB_Y               = "\uE0E5";
constexpr const char* ICON_KB_Z               = "\uE0E7";
constexpr const char* ICON_MONITOR            = "\uE32E";
constexpr const char* ICON_MICROPHONE         = "\uE326";
constexpr const char* ICON_CAMERA             = "\uE10E";
constexpr const char* ICON_PLUGS_CONNECTED    = "\uEB5A";
constexpr const char* ICON_WIFI               = "\uE4EA";
constexpr const char* ICON_WIFI_NONE          = "\uE4F0";
constexpr const char* ICON_WIFI_LOW           = "\uE4EC";
constexpr const char* ICON_WIFI_MEDIUM        = "\uE4EE";
constexpr const char* ICON_WIFI_HIGH          = "\uE4EA";
constexpr const char* ICON_BLUETOOTH          = "\uE0DA";
constexpr const char* ICON_ETHERNET           = "\uEDDE";
constexpr const char* ICON_USB                = "\uE956";
constexpr const char* ICON_VECTOR_THREE       = "\uEE62";
constexpr const char* ICON_DEVICE_MOBILE      = "\uE1E0";
constexpr const char* ICON_WRENCH             = "\uE5D4";
constexpr const char* ICON_BARCODE            = "\uE0B8";
constexpr const char* ICON_FINGERPRINT        = "\uE23E";
constexpr const char* ICON_MEMORY             = "\uE9C4";
constexpr const char* ICON_HARDDRIVE          = "\uE29E";
constexpr const char* ICON_GLOBE              = "\uE288";
constexpr const char* ICON_LINUX_LOGO         = "\uEB02";
constexpr const char* ICON_PACKAGE            = "\uE390";
constexpr const char* ICON_TIMER              = "\uE492";
constexpr const char* ICON_CPU                = "\uE610";
constexpr const char* ICON_BAT_FULL           = "\uE7C4";
constexpr const char* ICON_BAT_HIGH           = "\uE7C2";
constexpr const char* ICON_BAT_MEDIUM         = "\uE7C0";
constexpr const char* ICON_BAT_LOW            = "\uE7BE";
constexpr const char* ICON_BAT_EMPTY          = "\uE7C6";
constexpr const char* ICON_LIGHTNING          = "\uE2DE";
constexpr const char* ICON_GAUGE              = "\uE628";
constexpr const char* ICON_PULSE              = "\uE000";
constexpr const char* ICON_HEART              = "\uE2A8";
constexpr const char* ICON_THERMOMETER        = "\uE5C6";
constexpr const char* ICON_REPEAT             = "\uE3F6";
constexpr const char* ICON_TAG                = "\uE478";
constexpr const char* ICON_SCAN               = "\uEBB6";
constexpr const char* ICON_LINK_SIMPLE_HOR    = "\uE2EA";
constexpr const char* ICON_ARROWS_CLOCKWISE   = "\uE094";
constexpr const char* ICON_GEAR_FINE          = "\uE87C";
constexpr const char* ICON_BROADCAST          = "\uE0F2";
constexpr const char* ICON_PAPER_PLANE        = "\uE394";
constexpr const char* ICON_ENVELOPE_OPEN      = "\uE216";
constexpr const char* ICON_SIDEBAR            = "\uEAB6";
constexpr const char* ICON_CHART_BAR_HOR      = "\uE152";
constexpr const char* ICON_TRANSLATE          = "\uE4A2";
constexpr const char* ICON_CARET_DOWN         = "\uE136";

// Naive UI 3 Color Scheme
// ====================
// Common
// ====================
constexpr uint32_t LIGHT_BODY          = 0xFFFFFF;
constexpr uint32_t LIGHT_CARD          = 0xFFFFFF;
constexpr uint32_t LIGHT_ACTION        = 0xFAFAFC;
constexpr uint32_t LIGHT_BUTTON        = 0xF5F5F5;
constexpr uint32_t LIGHT_BORDER        = 0xE0E0E6;
constexpr uint32_t LIGHT_TEXT_PRIMARY  = 0x1F2225;
constexpr uint32_t LIGHT_TEXT_MUTED    = 0x767C82;
constexpr uint32_t LIGHT_TEXT_DISABLED = 0xC2C2C2;
constexpr uint32_t DARK_BODY           = 0x101014;
constexpr uint32_t DARK_CARD           = 0x18181C;
constexpr uint32_t DARK_ACTION         = 0x0F0F0F;
constexpr uint32_t DARK_BUTTON         = 0x141414;
constexpr uint32_t DARK_BORDER         = 0x3D3D3D;
constexpr uint32_t DARK_TEXT_PRIMARY   = 0xE6E6E6;
constexpr uint32_t DARK_TEXT_MUTED     = 0xA3A3A3;
constexpr uint32_t DARK_TEXT_DISABLED  = 0x616161;
// ====================
// Primary
// ====================
constexpr uint32_t LIGHT_PRIMARY         = 0x18A058;
constexpr uint32_t LIGHT_PRIMARY_HOVER   = 0x36AD6A;
constexpr uint32_t LIGHT_PRIMARY_PRESSED = 0x0C7A43;
constexpr uint32_t DARK_PRIMARY          = 0x63E2B7;
constexpr uint32_t DARK_PRIMARY_HOVER    = 0x7FE7C4;
constexpr uint32_t DARK_PRIMARY_PRESSED  = 0x5ACEA7;
// ====================
// Success
// ====================
constexpr uint32_t LIGHT_SUCCESS         = 0x18A058;
constexpr uint32_t LIGHT_SUCCESS_HOVER   = 0x36AD6A;
constexpr uint32_t LIGHT_SUCCESS_PRESSED = 0x0C7A43;
constexpr uint32_t DARK_SUCCESS          = 0x63E2B7;
constexpr uint32_t DARK_SUCCESS_HOVER    = 0x7FE7C4;
constexpr uint32_t DARK_SUCCESS_PRESSED  = 0x5ACEA7;
// ====================
// Info
// ====================
constexpr uint32_t LIGHT_INFO         = 0x2080F0;
constexpr uint32_t LIGHT_INFO_HOVER   = 0x4098FC;
constexpr uint32_t LIGHT_INFO_PRESSED = 0x1060C9;
constexpr uint32_t DARK_INFO          = 0x70C0E8;
constexpr uint32_t DARK_INFO_HOVER    = 0x8ACBEC;
constexpr uint32_t DARK_INFO_PRESSED  = 0x66AFD3;
// ====================
// Warning
// ====================
constexpr uint32_t LIGHT_WARNING         = 0xF0A020;
constexpr uint32_t LIGHT_WARNING_HOVER   = 0xFCB040;
constexpr uint32_t LIGHT_WARNING_PRESSED = 0xC97C10;
constexpr uint32_t DARK_WARNING          = 0xF2C97D;
constexpr uint32_t DARK_WARNING_HOVER    = 0xF5D599;
constexpr uint32_t DARK_WARNING_PRESSED  = 0xE6C260;
// ====================
// Error
// ====================
constexpr uint32_t LIGHT_ERROR         = 0xD03050;
constexpr uint32_t LIGHT_ERROR_HOVER   = 0xDE576D;
constexpr uint32_t LIGHT_ERROR_PRESSED = 0xAB1F3F;
constexpr uint32_t DARK_ERROR          = 0xE88080;
constexpr uint32_t DARK_ERROR_HOVER    = 0xE98B8B;
constexpr uint32_t DARK_ERROR_PRESSED  = 0xE57272;
}  // namespace view

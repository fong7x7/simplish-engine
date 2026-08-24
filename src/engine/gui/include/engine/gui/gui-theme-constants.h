#pragma once

#include "gui-color.h"

namespace eng {

/// Standard editor dark theme colors.
/// Used by all UI components for consistent appearance.
/// @thread_safety Main thread only — constants are immutable after init.
inline constexpr GuiColor THEME_BG{30, 30, 34};
inline constexpr GuiColor THEME_PANEL{40, 40, 44};
inline constexpr GuiColor THEME_BTN{55, 55, 60};
inline constexpr GuiColor THEME_BTN_HOVER{70, 70, 75};
inline constexpr GuiColor THEME_ACCENT{0, 122, 204};
inline constexpr GuiColor THEME_ACCENT_HOVER{0, 140, 230};
inline constexpr GuiColor THEME_TEXT{200, 200, 200};
inline constexpr GuiColor THEME_DIM{120, 120, 120};
inline constexpr GuiColor THEME_BORDER{55, 55, 59};
inline constexpr GuiColor THEME_HOVER{50, 50, 55};
inline constexpr GuiColor THEME_OVERLAY{0, 0, 0, 180};
inline constexpr GuiColor THEME_ERROR{220, 60, 60};
inline constexpr GuiColor THEME_DANGER{180, 60, 60};

/// Standard corner radius for buttons.
inline constexpr float THEME_BTN_RADIUS = 5.0f;

}  // namespace eng

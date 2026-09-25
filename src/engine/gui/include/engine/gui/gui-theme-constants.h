#pragma once

/// @file gui-theme-constants.h
/// @brief The dark palette's colours as constants, for code that draws
/// without a `GuiDrawContext` to ask for its theme.
/// @par Threading
/// Constants only.

#include "gui-color.h"
#include "gui-palette.h"

namespace eng {

// Prefer `ctx.activeTheme().palette` in anything that draws: these follow
// the dark palette and nothing else, so code using them will not change
// with the theme. They exist for the editor's hand-drawn widgets and for
// tests that match captured pixels.
inline constexpr GuiColor THEME_BG = GUI_PALETTE_DARK.background;
inline constexpr GuiColor THEME_PANEL = GUI_PALETTE_DARK.surface;
inline constexpr GuiColor THEME_BTN = GUI_PALETTE_DARK.control;
inline constexpr GuiColor THEME_BTN_HOVER = GUI_PALETTE_DARK.control_hover;
inline constexpr GuiColor THEME_ACCENT = GUI_PALETTE_DARK.primary;
inline constexpr GuiColor THEME_ACCENT_HOVER = GUI_PALETTE_DARK.primary_hover;
inline constexpr GuiColor THEME_TEXT = GUI_PALETTE_DARK.text;
inline constexpr GuiColor THEME_DIM = GUI_PALETTE_DARK.text_muted;
inline constexpr GuiColor THEME_BORDER = GUI_PALETTE_DARK.border;
inline constexpr GuiColor THEME_DANGER = GUI_PALETTE_DARK.danger;
/// A row under the pointer in a hand-drawn list.
inline constexpr GuiColor THEME_HOVER{50, 50, 55};
/// Dims what is behind a hand-drawn modal.
inline constexpr GuiColor THEME_OVERLAY{0, 0, 0, 180};
/// Error text.
inline constexpr GuiColor THEME_ERROR{220, 60, 60};
/// Standard corner radius for buttons: the theme's `GuiRadius::MD`.
inline constexpr float THEME_BTN_RADIUS = 5.0f;

}  // namespace eng

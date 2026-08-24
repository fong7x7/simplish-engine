#pragma once

#include "gui-color.h"

namespace eng {

/// Centralized style definition for all UI components.
/// Provides a single source of truth for colors, radii, and dimensions.
/// Use GuiStyle::dark() or GuiStyle::light() for pre-built presets, or
/// create custom styles. Components read from a shared GuiStyle pointer
/// when available, falling back to per-instance fields when null.
/// @thread_safety Main thread only (read-only after construction).
struct GuiStyle {
  // ─── Global ───────────────────────────────────────────
  /// Window/editor background color.
  GuiColor background{};
  /// Panel background color.
  GuiColor panel{};
  /// Border line color.
  GuiColor border{};
  /// Semi-transparent overlay color.
  GuiColor overlay{};
  /// Primary text color.
  GuiColor text{};
  /// Secondary/dimmed text color.
  GuiColor text_dim{};
  /// Error indicator color.
  GuiColor error{};

  // ─── Button ───────────────────────────────────────────
  /// Button normal background.
  GuiColor btn_bg{};
  /// Button hovered background.
  GuiColor btn_bg_hover{};
  /// Button label text color.
  GuiColor btn_text{};
  /// Accent/primary button background.
  GuiColor btn_accent_bg{};
  /// Accent button hovered background.
  GuiColor btn_accent_bg_hover{};
  /// Destructive action button background.
  GuiColor btn_danger_bg{};
  /// Standard button corner radius.
  float btn_corner_radius = 5.0f;

  // ─── Text Input ───────────────────────────────────────
  /// Input field background color.
  GuiColor input_bg{};
  /// Input text color.
  GuiColor input_text{};
  /// Input border color.
  GuiColor input_border{};
  /// Input border color when focused (accent highlight).
  GuiColor input_border_focused{};
  /// Selection highlight background color.
  GuiColor input_selection_bg{};
  /// Selected text color drawn over the highlight.
  GuiColor input_selection_text{};
  /// Input corner radius (0 = sharp corners).
  float input_corner_radius = 0.0f;

  // ─── Dropdown ─────────────────────────────────────────
  /// Dropdown panel background.
  GuiColor dropdown_bg{};
  /// Dropdown item text color.
  GuiColor dropdown_text{};
  /// Dropdown hovered item highlight.
  GuiColor dropdown_hover{};
  /// Default dropdown width in logical pixels.
  int dropdown_width = 140;
  /// Default dropdown item row height in logical pixels.
  int dropdown_item_height = 24;

  // ─── Slider ─────────────────────────────────────────
  /// Slider track background color.
  GuiColor slider_track{};
  /// Slider filled portion color.
  GuiColor slider_fill{};
  /// Slider handle color.
  GuiColor slider_handle{};
  /// Slider handle color when hovered or dragging.
  GuiColor slider_handle_hover{};
  /// Slider handle width and height in logical pixels.
  float slider_handle_size = 12.0f;
  /// Slider track bar height in logical pixels.
  float slider_track_height = 4.0f;

  // ─── Hover ────────────────────────────────────────────
  /// Generic hover highlight color.
  GuiColor hover_highlight{};

  /// Dark mode preset matching the current editor theme.
  /// @thread_safety Thread-safe (returns reference to static const).
  static const GuiStyle& dark();

  /// Light mode preset.
  /// @thread_safety Thread-safe (returns reference to static const).
  static const GuiStyle& light();
};

}  // namespace eng

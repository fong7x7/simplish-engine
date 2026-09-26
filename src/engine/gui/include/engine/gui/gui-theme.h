#pragma once

/// @file gui-theme.h
/// @brief A GUI's design tokens and every built-in widget's look in each
/// state, derived from one palette.
/// @par Threading
/// Main thread only; the presets are immutable and safe to read anywhere.

#include "gui-button-variant.h"
#include "gui-elevation.h"
#include "gui-font.h"
#include "gui-palette.h"
#include "gui-radius.h"
#include "gui-shadow.h"
#include "gui-space.h"
#include "gui-state-styles.h"
#include "gui-text-role.h"
#include "gui-text-size.h"

#include <array>
#include <string>

namespace eng {

/// Everything a GUI's look comes from: a palette, scales for spacing,
/// corner radii, type and elevation, how long state changes take to blend,
/// and — derived from those — each built-in widget's look in every state.
///
/// A widget draws from the theme on its `GuiDrawContext`
/// (`ctx.activeTheme()`), never from colour constants: a light theme, a
/// game's own look, or a project's `theme.json` is then a different
/// `GuiTheme` and no different code. See `technical/theming.md`.
struct GuiTheme {
  /// Shown in logs and tools.
  std::string name{};
  /// The colours, by role.
  GuiPalette palette{};
  /// Pixels for each `GuiSpace` step.
  std::array<float, GUI_SPACE_COUNT> spacing = GUI_DEFAULT_SPACING;
  /// Pixels for each `GuiRadius` step.
  std::array<float, GUI_RADIUS_COUNT> radii = GUI_DEFAULT_RADII;
  /// Pixel sizes for each `GuiTextSize` step.
  std::array<float, GUI_TEXT_SIZE_COUNT> text_sizes = GUI_DEFAULT_TEXT_SIZES;
  /// The font for each `GuiTextRole`, derived from `text_sizes`.
  std::array<GuiFont, GUI_TEXT_ROLE_COUNT> type_roles{};
  /// The shadow for each `GuiElevation`.
  std::array<GuiShadow, GUI_ELEVATION_COUNT> shadows{};
  /// Seconds a widget takes to blend from one state's look to the next.
  float transition_seconds = 0.12f;
  /// Each `GuiButtonVariant`'s look.
  std::array<GuiStateStyles, GUI_BUTTON_VARIANT_COUNT> buttons{};
  /// Text fields and areas.
  GuiStateStyles field{};
  /// A raised surface: a card, a dialog body.
  GuiStateStyles card{};
  /// A dropdown or menu's box.
  GuiStateStyle menu{};
  /// Width of the focus ring, in logical pixels.
  float focus_ring_width = 2.0f;

  /// Pixels for @p step.
  [[nodiscard]] float space(GuiSpace step) const;
  /// Pixels for @p step.
  [[nodiscard]] float radius(GuiRadius step) const;
  /// Pixel size for @p step.
  [[nodiscard]] float textSize(GuiTextSize step) const;
  /// The font @p role is set in.
  [[nodiscard]] const GuiFont& font(GuiTextRole role) const;
  /// The shadow for @p level.
  [[nodiscard]] const GuiShadow& shadow(GuiElevation level) const;
  /// @p variant's look.
  [[nodiscard]] const GuiStateStyles& button(GuiButtonVariant variant) const;

  /// Recompute the shadows, the role fonts and every component style from
  /// the palette, the radii and the type scale, after changing any.
  void deriveComponents();

  /// A theme named @p name built on @p palette, with the default scales.
  [[nodiscard]] static GuiTheme fromPalette(const GuiPalette& palette,
                                            std::string name);
  /// The dark theme: the editor's, and the default.
  [[nodiscard]] static const GuiTheme& dark();
  /// The light theme.
  [[nodiscard]] static const GuiTheme& light();
};

}  // namespace eng

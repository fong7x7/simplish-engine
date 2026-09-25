#pragma once

/// @file gui-state-styles.h
/// @brief A widget kind's look in every interaction state.
/// @par Threading
/// Immutable value type.

#include "gui-state-style.h"
#include "gui-widget-state.h"

#include <array>

namespace eng {

/// One `GuiStateStyle` per `GuiWidgetState`: how a kind of widget — a
/// primary button, a text field — looks at rest, hovered, pressed, focused,
/// disabled and selected. Themes hold one per component; a widget can
/// point at its own to be styled differently.
struct GuiStateStyles {
  /// Indexed by `GuiWidgetState`.
  std::array<GuiStateStyle, GUI_WIDGET_STATE_COUNT> states{};

  /// The look for @p state.
  [[nodiscard]] const GuiStateStyle& of(GuiWidgetState state) const;
  /// The look for @p state, to set it.
  [[nodiscard]] GuiStateStyle& of(GuiWidgetState state);

  /// Every state looking like @p style — the start of a hand-built set.
  [[nodiscard]] static GuiStateStyles uniform(const GuiStateStyle& style);
};

}  // namespace eng

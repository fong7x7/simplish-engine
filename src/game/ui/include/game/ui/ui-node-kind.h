#pragma once

/// @file ui-node-kind.h
/// @brief What one node of a game screen is.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The widgets a screen file can name — a closed set, each built from the
/// engine's own GUI widgets (docs/game/ui.md).
enum class UiNodeKind : uint8_t {
  /// A box that lays its children out by flexbox; a `GuiPanel`.
  PANEL,
  /// A line of text, which may show values: `"Score: {score}"`.
  LABEL,
  /// A button that, pressed, chooses its action.
  BUTTON,
  /// A bar filled by one value's share of another: health, a timer.
  BAR,
  /// Empty space; grows to fill its line unless told otherwise.
  SPACER,
  /// A box and its text that, pressed, chooses its action; shows its
  /// `checked` value, never its own guess.
  CHECKBOX,
  /// A switch and its text, the same way.
  TOGGLE,
};

/// Whether a node of @p kind is pressed to choose its action: a button, a
/// checkbox or a toggle.
[[nodiscard]] constexpr bool uiChooses(UiNodeKind kind) {
  return kind == UiNodeKind::BUTTON || kind == UiNodeKind::CHECKBOX ||
         kind == UiNodeKind::TOGGLE;
}

}  // namespace eng::game

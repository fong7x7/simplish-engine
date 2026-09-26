#pragma once

/// @file ui-bindings.h
/// @brief Which of a screen node's flags follow the values the logic sets.
/// @par Threading
/// A value type.

#include <string>

namespace eng::game {

/// A node's flags bound to values: each is a key, on while that value is
/// set to anything but empty, `0` or `false` — or `!key`, on while it is
/// not. Empty leaves the flag as it is built: shown, enabled, not
/// selected, not checked.
struct UiBindings {
  /// Shown only while this holds; hidden, it takes no room.
  std::string visible{};
  /// Dimmed and not pressable while this holds.
  std::string disabled{};
  /// Drawn chosen while this holds: the current tab, the upgrade picked.
  std::string selected{};
  /// A checkbox or toggle is on while this holds.
  std::string checked{};
};

}  // namespace eng::game

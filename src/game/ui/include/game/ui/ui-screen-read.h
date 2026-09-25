#pragma once

/// @file ui-screen-read.h
/// @brief A screen file read, and what was wrong with it.
/// @par Threading
/// A value type.

#include <game/ui/ui-screen.h>
#include <optional>
#include <string>
#include <vector>

namespace eng::game {

/// What `parseUiScreen` made of a file: the screen, unless it could not be
/// read at all, and every problem found — each naming where, so an author
/// can fix it. A node with a problem is left out or read with its default;
/// the rest of the screen stands.
struct UiScreenRead {
  /// The screen; nothing when the text is not a screen at all.
  std::optional<UiScreen> screen{};
  /// What was wrong, each as `path: what` — `root/children[2]: ...`.
  std::vector<std::string> problems{};
};

}  // namespace eng::game

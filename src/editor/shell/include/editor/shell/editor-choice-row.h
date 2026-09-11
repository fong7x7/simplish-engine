#pragma once

/// @file editor-choice-row.h
/// @brief One choice row of the properties panel: its names and its pick.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/editor-choice-kind.h>
#include <string>
#include <vector>

namespace eng::editor {

/// A row that steps through names, wrapping round, and shows one.
/// @thread_safety Main-thread-only.
struct EditorChoiceRow {
  /// Which row it is.
  EditorChoiceKind kind = EditorChoiceKind::ANIMATION;
  /// The names it steps through, at least one.
  std::vector<std::string> names{};
  /// Which of them it shows.
  size_t current = 0;
};

}  // namespace eng::editor

#pragma once

/// @file editor-sheet-choices.h
/// @brief What a sprite billboard's Sheet row offers.
/// @par Threading Immutable value type.

#include <cstddef>
#include <string>
#include <vector>

namespace eng::editor {

/// The sprite sheets a billboard can be pointed at, in the order the Sheet
/// row steps through them, and which one it shows.
///
/// The same shape as `EditorEffectChoices`, and for the same reason: the
/// row shows a name a designer reads and reports an index, and what that
/// index means — here a path under the project's assets — is the editor's
/// business rather than the panel's.
/// @thread_safety Immutable value type.
struct EditorSheetChoices {
  /// What the row shows for each: the sheet's path without its extension.
  std::vector<std::string> names{};
  /// The path each choice points the billboard at, alongside `names`.
  std::vector<std::string> paths{};
  /// Which choice the billboard shows.
  size_t current = 0;
};

}  // namespace eng::editor

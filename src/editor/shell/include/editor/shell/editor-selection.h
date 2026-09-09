#pragma once

/// @file editor-selection.h
/// @brief What the editor has selected, as an index into the placements.
/// @par Threading Main-thread-only.

namespace eng::editor {

/// No placement is selected, and no placement was picked.
///
/// A signed index with a sentinel rather than an optional: the selection is
/// carried through widget callbacks and compared against list positions on
/// every edit, and one number that every one of those understands is worth
/// more here than the type telling the story.
inline constexpr int EDITOR_PLACEMENT_NONE = -1;

}  // namespace eng::editor

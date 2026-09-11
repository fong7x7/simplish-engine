#pragma once

/// @file editor-selection.h
/// @brief What the editor has selected, and which list it lives in.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <cstdint>

namespace eng::editor {

/// No placement is selected, and no placement was picked.
///
/// A signed index with a sentinel rather than an optional: this is what the
/// viewport reports from a pick, where one number every callback
/// understands is worth more than the type telling the story.
inline constexpr int EDITOR_PLACEMENT_NONE = -1;

/// Which of the document's lists a selection names.
/// @thread_safety Immutable value type.
enum class EditorSelectionKind : uint8_t {
  /// Nothing is selected, and `EditorSelection::index` means nothing.
  NONE,
  /// A placed asset, indexed into `EditorDocument::placements`.
  PLACEMENT,
  /// A light, indexed into `EditorDocument::lights`.
  LIGHT,
  /// A player start, indexed into `EditorDocument::player_starts`.
  PLAYER_START,
  /// A patrol route's waypoint, indexed into `EditorDocument::waypoints`.
  WAYPOINT,
};

/// The one thing the properties panel is editing.
///
/// A kind and an index rather than two nullable indices: there is exactly
/// one selection, and a record that cannot hold two of them at once is a
/// record that cannot disagree with itself about which panel to show.
/// @thread_safety Immutable value type.
struct EditorSelection {
  /// Which list `index` refers to, or `NONE`.
  EditorSelectionKind kind = EditorSelectionKind::NONE;
  /// Position in that list. Meaningless while `kind` is `NONE`.
  size_t index = 0;
};

/// Whether @p selection names something of @p kind.
[[nodiscard]] inline bool selectionIs(const EditorSelection& selection,
                                      EditorSelectionKind kind) {
  return selection.kind == kind;
}

}  // namespace eng::editor

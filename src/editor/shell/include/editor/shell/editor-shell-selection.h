#pragma once

/// @file editor-shell-selection.h
/// @brief Whether what is selected is there, ground areas included.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-selection.h>
#include <editor/shell/editor-shell-state.h>
#include <optional>

namespace eng::editor {

/// How many entries a selection of @p kind can name in @p state: the
/// length of the document's list for it, or — for a ground area — one when
/// an area is selected and none otherwise. A selection is there exactly
/// when its index is below this.
[[nodiscard]] size_t editorSelectableCount(const EditorShellState& state,
                                           EditorSelectionKind kind);

/// Select the painted area the cell at @p cell belongs to in @p state, and
/// say whether there was one: false, leaving the selection alone, on bare
/// ground.
bool selectEditorGround(EditorShellState& state, GroundCell cell);

/// The terrain the selected ground area is painted with — its first
/// cell's — or nothing when no area is selected.
[[nodiscard]] std::optional<uint8_t>
editorSelectedTerrain(const EditorShellState& state);

}  // namespace eng::editor

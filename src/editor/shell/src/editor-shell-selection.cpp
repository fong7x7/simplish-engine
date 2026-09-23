#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-shell-selection.h>
#include <engine/render-ground/ground-region.h>

namespace eng::editor {

size_t editorSelectableCount(const EditorShellState& state,
                             EditorSelectionKind kind) {
  if (kind == EditorSelectionKind::GROUND) {
    return state.ground_selection.empty() ? 0 : 1;
  }
  return editorListSize(state.document, kind);
}

bool selectEditorGround(EditorShellState& state, GroundCell cell) {
  std::vector<GroundCell> area =
      connectedGroundCells(state.document.ground, cell);
  if (area.empty()) {
    return false;
  }
  state.ground_selection = std::move(area);
  state.selection = {EditorSelectionKind::GROUND, 0};
  return true;
}

std::optional<uint8_t> editorSelectedTerrain(const EditorShellState& state) {
  if (state.selection.kind != EditorSelectionKind::GROUND ||
      state.ground_selection.empty()) {
    return std::nullopt;
  }
  return state.document.ground.at(state.ground_selection.front());
}

}  // namespace eng::editor

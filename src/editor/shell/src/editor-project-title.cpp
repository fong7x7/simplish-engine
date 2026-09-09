#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-project-title.h>
#include <string>

namespace eng::editor {

std::string editorProjectDisplayName(const EditorShellState& state) {
  if (!state.project.loaded) {
    return std::string(EDITOR_NO_PROJECT_NAME);
  }
  std::string name = state.project.metadata.name;
  if (hasUnsavedEditorChanges(state.history)) {
    name += EDITOR_UNSAVED_MARK;
  }
  return name;
}

std::string editorProjectTitle(const EditorShellState& state) {
  if (!state.project.loaded) {
    return std::string(EDITOR_APP_TITLE);
  }
  return std::string(EDITOR_APP_TITLE) + " — " +
         editorProjectDisplayName(state);
}

}  // namespace eng::editor

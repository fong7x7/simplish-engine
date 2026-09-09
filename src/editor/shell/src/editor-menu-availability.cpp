#include <algorithm>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-menu-availability.h>

namespace eng::editor {

bool editorMenuCommandImplemented(EditorMenuCommand command) {
  return std::find(std::begin(EDITOR_IMPLEMENTED_COMMANDS),
                   std::end(EDITOR_IMPLEMENTED_COMMANDS),
                   command) != std::end(EDITOR_IMPLEMENTED_COMMANDS);
}

bool editorMenuCommandEnabled(const EditorShellState& state,
                              EditorMenuCommand command) {
  // The state-dependent rows first: each is built, and each would still do
  // nothing if it were live right now.
  if (command == EditorMenuCommand::CLOSE_PROJECT ||
      command == EditorMenuCommand::SAVE ||
      command == EditorMenuCommand::SET_VIEW_DIMETRIC ||
      command == EditorMenuCommand::SET_VIEW_ISOMETRIC) {
    // Saving and switching projection both write into the project's own
    // directory, so with no project open there is nowhere for either to go.
    return state.project.loaded;
  }
  if (command == EditorMenuCommand::UNDO) {
    return canUndoEditorAction(state.history);
  }
  if (command == EditorMenuCommand::REDO) {
    return canRedoEditorAction(state.history);
  }
  if (command == EditorMenuCommand::DELETE_SELECTION) {
    // Live only when something is selected that a removal would actually
    // reach — the same question the key and the agent's tool both ask, so
    // the greyed row and the refused call never disagree.
    return editorDeleteAction(state.document, state.selection).has_value();
  }
  return editorMenuCommandImplemented(command);
}

}  // namespace eng::editor

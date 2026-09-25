#include <algorithm>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-menu-availability.h>
#include <editor/shell/editor-shell-selection.h>

namespace eng::editor {

namespace {

  /// Whether Delete reaches anything: an entry a removal takes out, or an
  /// area of ground it erases.
  bool canDelete(const EditorShellState& state) {
    return editorDeleteAction(state.document, state.selection).has_value() ||
           ((selectionIs(state.selection, EditorSelectionKind::GROUND) ||
             selectionIs(state.selection, EditorSelectionKind::WATER)) &&
            editorSelectableCount(state, state.selection.kind) > 0);
  }

  /// Whether a Build menu @p command would do anything: a project is open,
  /// no build is running — one at a time — and, to scaffold, the project
  /// has no logic yet.
  bool buildCommandEnabled(const EditorShellState& state,
                           EditorMenuCommand command) {
    return state.project.loaded &&
           state.build.status != EditorBuildStatus::RUNNING &&
           (command != EditorMenuCommand::NEW_GAME_LOGIC ||
            !state.build.has_logic);
  }

  /// Whether a @p command that needs a project would do anything.
  bool projectCommandEnabled(const EditorShellState& state,
                             EditorMenuCommand command) {
    return editorMenuCommandBuilds(command)
               ? buildCommandEnabled(state, command)
               : state.project.loaded;
  }

}  // namespace

bool editorMenuCommandNeedsProject(EditorMenuCommand command) {
  return command == EditorMenuCommand::CLOSE_PROJECT ||
         command == EditorMenuCommand::SAVE ||
         command == EditorMenuCommand::NEW_LEVEL ||
         command == EditorMenuCommand::SET_VIEW_DIMETRIC ||
         command == EditorMenuCommand::SET_VIEW_ISOMETRIC ||
         command == EditorMenuCommand::SET_SHADING_SMOOTH ||
         command == EditorMenuCommand::SET_SHADING_CEL ||
         command == EditorMenuCommand::PLAYTEST ||
         command == EditorMenuCommand::IMPORT_SOUND ||
         editorMenuCommandBuilds(command) || editorStandInsOf(command) >= 0;
}

bool editorMenuCommandBuilds(EditorMenuCommand command) {
  return command == EditorMenuCommand::NEW_GAME_LOGIC ||
         command == EditorMenuCommand::BUILD_GAME_LOGIC ||
         command == EditorMenuCommand::DEPLOY_GAME;
}

bool editorMenuCommandNeedsPlaytest(EditorMenuCommand command) {
  return command == EditorMenuCommand::PAUSE_PLAYTEST ||
         command == EditorMenuCommand::STEP_PLAYTEST;
}

bool editorMenuCommandImplemented(EditorMenuCommand command) {
  return std::find(std::begin(EDITOR_IMPLEMENTED_COMMANDS),
                   std::end(EDITOR_IMPLEMENTED_COMMANDS),
                   command) != std::end(EDITOR_IMPLEMENTED_COMMANDS);
}

bool editorMenuCommandEnabled(const EditorShellState& state,
                              EditorMenuCommand command) {
  // The state-dependent rows first: each is built, and each would still do
  // nothing if it were live right now.
  if (editorMenuCommandNeedsProject(command)) {
    return projectCommandEnabled(state, command);
  }
  if (editorMenuCommandNeedsPlaytest(command)) {
    return state.playtest.mode == EditorPlayMode::PLAYING;
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
    return canDelete(state);
  }
  return editorMenuCommandImplemented(command);
}

}  // namespace eng::editor

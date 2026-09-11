#pragma once

/// @file editor-menu-availability.h
/// @brief Which menu commands are live, and why.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-menu-command.h>
#include <editor/shell/editor-shell-state.h>

namespace eng::editor {

/// Commands the editor can actually carry out today.
///
/// Everything else in `EditorMenuCommand` is listed in the menu and
/// rendered disabled — a greyed row is a promise about the roadmap, a
/// missing row is a lie about it. Being here is necessary for a row to be
/// live, not sufficient: `editorMenuCommandEnabled` gates some of these
/// further on what the editor has open.
inline constexpr EditorMenuCommand EDITOR_IMPLEMENTED_COMMANDS[] = {
    EditorMenuCommand::NEW_PROJECT,
    EditorMenuCommand::OPEN_PROJECT,
    EditorMenuCommand::SAVE,
    EditorMenuCommand::NEW_LEVEL,
    EditorMenuCommand::CLOSE_PROJECT,
    EditorMenuCommand::EXIT,
    EditorMenuCommand::UNDO,
    EditorMenuCommand::REDO,
    EditorMenuCommand::DELETE_SELECTION,
    EditorMenuCommand::RESET_VIEW,
    EditorMenuCommand::ZOOM_IN,
    EditorMenuCommand::ZOOM_OUT,
    EditorMenuCommand::TOGGLE_GRID,
    EditorMenuCommand::SET_VIEW_DIMETRIC,
    EditorMenuCommand::SET_VIEW_ISOMETRIC,
    EditorMenuCommand::SET_SHADING_SMOOTH,
    EditorMenuCommand::SET_SHADING_CEL,
    EditorMenuCommand::ABOUT,
    EditorMenuCommand::PLAYTEST,
    EditorMenuCommand::PAUSE_PLAYTEST,
    EditorMenuCommand::STEP_PLAYTEST,
    EditorMenuCommand::TOGGLE_NAVIGATION,
    EditorMenuCommand::TOGGLE_AI_OVERLAY,
};

/// Whether @p command names work that exists at all.
[[nodiscard]] bool editorMenuCommandImplemented(EditorMenuCommand command);

/// Whether @p command writes into the open project, and so has nowhere to
/// go until one is open. The menu bar, which sees no `EditorShellState`,
/// reads this directly; everything else asks `editorMenuCommandEnabled`.
[[nodiscard]] bool editorMenuCommandNeedsProject(EditorMenuCommand command);

/// Whether @p command acts on a running playtest, and so does nothing
/// while the level is being edited. The menu bar reads this directly, as it
/// does `editorMenuCommandNeedsProject`.
[[nodiscard]] bool editorMenuCommandNeedsPlaytest(EditorMenuCommand command);

/// Whether @p command would do anything against @p state right now.
///
/// One rule, read by both the menu bar — which greys the row — and the
/// agent API, which refuses the call. Two copies of this would eventually
/// disagree, and an agent told a command is live when the menu greys it
/// would be right to be confused about which one is lying.
[[nodiscard]] bool editorMenuCommandEnabled(const EditorShellState& state,
                                            EditorMenuCommand command);

}  // namespace eng::editor

#pragma once

/// @file editor-level-ops.h
/// @brief Creating a level, and opening one the project already holds.
/// @par Threading Main-thread-only (touches the filesystem).

#include <editor/shell/editor-level-result.h>
#include <editor/shell/editor-level-status.h>
#include <editor/shell/editor-level-unsaved.h>
#include <editor/shell/editor-shell-state.h>
#include <string>
#include <string_view>

namespace eng::editor {

/// Longest level id the editor will make or accept.
///
/// An id becomes a C++ identifier in generated content ([project-format.md
/// §3]), so it has to be a plausible one; the limit is here to keep a
/// pasted paragraph out of a file name, not because the format states one.
inline constexpr size_t EDITOR_LEVEL_ID_MAX = 64;

/// Whether @p id is a level id the project format allows: lowercase
/// letters, digits and underscores, starting with a letter.
///
/// The rule is the format's, not the filesystem's ([project-format.md §3]):
/// an id is the name generated C++ uses, so anything that is not a valid
/// identifier there cannot be one here either.
[[nodiscard]] bool isEditorLevelId(std::string_view id);

/// The level id @p text turns into: lowercased, with runs of anything else
/// becoming single underscores. Empty when nothing usable is left.
///
/// What the New Level dialog runs the typed name through, so somebody who
/// types "Transit Station" gets `transit_station` rather than a refusal.
[[nodiscard]] std::string editorLevelIdFromText(std::string_view text);

/// Whether creating the level @p id would work right now, and what stops
/// it when it would not.
///
/// The same rule the create itself runs, so the agent API can refuse a call
/// with a reason instead of queueing work the editor will decline — the
/// question `editorMenuCommandEnabled` answers for a menu row.
[[nodiscard]] EditorLevelStatus
canCreateEditorLevel(const EditorShellState& state, std::string_view id,
                     EditorLevelUnsaved unsaved);

/// Whether opening the level @p id would work right now, and what stops it
/// when it would not.
[[nodiscard]] EditorLevelStatus
canOpenEditorLevel(const EditorShellState& state, std::string_view id,
                   EditorLevelUnsaved unsaved);

/// Create the level @p id in the open project and open it.
///
/// Refuses an id the project already holds rather than emptying it. The
/// new level's file is written before the editor switches, so a level that
/// could not be written is not one the editor is left sitting in.
[[nodiscard]] EditorLevelResult createEditorLevel(EditorShellState& state,
                                                  std::string_view id,
                                                  EditorLevelUnsaved unsaved);

/// Open the project's level @p id, replacing what the editor holds.
///
/// The document, the selection and the undo history all belong to the level
/// being closed, so all three go with it: an action naming a placement
/// index would otherwise undo into a level that never had it.
[[nodiscard]] EditorLevelResult openEditorLevel(EditorShellState& state,
                                                std::string_view id,
                                                EditorLevelUnsaved unsaved);

/// Point @p state at the level a freshly-opened project should start on:
/// `main` when it has one, otherwise the first level it does have.
///
/// Called after a project is opened and before its level is read, because
/// the previous project's level id is still in the state until it is.
void chooseEditorStartLevel(EditorShellState& state);

}  // namespace eng::editor

#pragma once

/// @file editor-level-io.h
/// @brief Writing the open project's level to disk and reading it back.
/// @par Threading Main-thread-only (touches the filesystem).

#include <editor/shell/editor-level-load.h>
#include <editor/shell/editor-shell-state.h>
#include <filesystem>
#include <optional>
#include <string_view>

namespace eng::editor {

/// What every level file is called after its id, as the format spells it
/// ([project-format.md §4]).
inline constexpr std::string_view EDITOR_LEVEL_FILE_SUFFIX = ".level.json";

/// Where the level @p id of the project rooted at @p root is written:
/// `<root>/content/levels/<id>.level.json`.
[[nodiscard]] std::filesystem::path
editorLevelPath(const std::filesystem::path& root, std::string_view id);

/// Where @p state's open level is written, which is the same path for the
/// level its `level_id` names.
[[nodiscard]] std::filesystem::path
editorLevelPath(const EditorShellState& state);

/// Whether the open level's file is there. A level nothing has been saved
/// into has none, which is an ordinary state and not a failure.
[[nodiscard]] bool editorLevelExists(const EditorShellState& state);

/// Write @p state's document to its project's level file, creating the
/// directories above it. False with no project open — there would be
/// nowhere to put it — or when the write fails.
///
/// Takes the whole shell state rather than its four relevant pieces
/// because that is what makes this testable the way the agent tools are:
/// against a bare `EditorShellState`, with no window and no GPU.
[[nodiscard]] bool saveEditorLevel(const EditorShellState& state);

/// Write an empty level file for @p id under @p root, creating the
/// directories above it. False when the write fails.
///
/// A level exists once its file does, so this is what makes a new one
/// something the next scan will find — before anything has been placed in
/// it, and before the editor has switched to it.
[[nodiscard]] bool createEditorLevelFile(const std::filesystem::path& root,
                                         std::string_view id);

/// Read the open level's file back, binding each prop to @p state's asset
/// list.
///
/// Nothing when no project is open, when there is no level file, or when
/// the file cannot be read or parsed. `editorLevelExists` is what separates
/// the second of those from the rest, since only one of them is worth
/// telling somebody about.
[[nodiscard]] std::optional<EditorLevelLoad>
loadEditorLevel(const EditorShellState& state);

}  // namespace eng::editor

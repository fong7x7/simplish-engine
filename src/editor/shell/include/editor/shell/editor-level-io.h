#pragma once

/// @file editor-level-io.h
/// @brief Writing the open project's level to disk and reading it back.
/// @par Threading Main-thread-only (touches the filesystem).

#include <editor/shell/editor-level-load.h>
#include <editor/shell/editor-shell-state.h>
#include <filesystem>
#include <optional>

namespace eng::editor {

/// Where the one level of the project rooted at @p root is written:
/// `<root>/content/levels/main.level.json`.
[[nodiscard]] std::filesystem::path
editorLevelPath(const std::filesystem::path& root);

/// Whether that file is there. A project that has never been saved has no
/// level file, which is an ordinary state and not a failure.
[[nodiscard]] bool editorLevelExists(const EditorShellState& state);

/// Write @p state's document to its project's level file, creating the
/// directories above it. False with no project open — there would be
/// nowhere to put it — or when the write fails.
///
/// Takes the whole shell state rather than its four relevant pieces
/// because that is what makes this testable the way the agent tools are:
/// against a bare `EditorShellState`, with no window and no GPU.
[[nodiscard]] bool saveEditorLevel(const EditorShellState& state);

/// Read that file back, binding each prop to @p state's asset list.
///
/// Nothing when no project is open, when there is no level file, or when
/// the file cannot be read or parsed. `editorLevelExists` is what separates
/// the second of those from the rest, since only one of them is worth
/// telling somebody about.
[[nodiscard]] std::optional<EditorLevelLoad>
loadEditorLevel(const EditorShellState& state);

}  // namespace eng::editor

#pragma once

/// @file editor-project-title.h
/// @brief How the chrome names the open project, unsaved marker included.
/// @par Threading Thread-safe (pure functions over shell state).

#include <editor/shell/editor-shell-state.h>
#include <string>
#include <string_view>

namespace eng::editor {

/// What the toolbar shows when no project is open.
inline constexpr std::string_view EDITOR_NO_PROJECT_NAME = "No project";

/// What the title bar reads with no project open, and the tail of what it
/// reads with one.
inline constexpr std::string_view EDITOR_APP_TITLE = "Simplish Editor";

/// The mark on a name whose level holds edits that are not in its file.
///
/// The asterisk every editor uses for the same thing, spaced off the name
/// rather than appended to it: `Demo *` reads as a marked project where
/// `Demo*` reads as a project called that.
inline constexpr std::string_view EDITOR_UNSAVED_MARK = " *";

/// The open project's name as the toolbar shows it, or
/// `EDITOR_NO_PROJECT_NAME` when none is open.
[[nodiscard]] std::string
editorProjectDisplayName(const EditorShellState& state);

/// The whole window and title-bar string: the application's name, and the
/// open project's after it.
[[nodiscard]] std::string editorProjectTitle(const EditorShellState& state);

}  // namespace eng::editor

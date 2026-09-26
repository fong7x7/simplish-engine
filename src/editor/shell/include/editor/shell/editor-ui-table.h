#pragma once

/// @file editor-ui-table.h
/// @brief A project's own game screens, read from `content/ui/`.
/// @par Threading
/// Main-thread-only; reads files.

#include <engine/gui/gui-theme.h>
#include <filesystem>
#include <game/content/game-content.h>
#include <game/ui/ui-screen.h>
#include <game/ui/ui-values.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The folder a project's screens are in, under `content/`.
inline constexpr std::string_view EDITOR_UI_DIR_NAME = "ui";

/// What a screen file's name ends in: `pause.ui.json` is the screen
/// `pause`.
inline constexpr std::string_view EDITOR_UI_FILE_SUFFIX = ".ui.json";

/// The file in `content/ui/` that themes every screen of a project.
inline constexpr std::string_view EDITOR_UI_THEME_FILE = "theme.json";

/// Every screen of a project (ADR-012, docs/game/ui.md), the theme they
/// are drawn in, and what was wrong with any of them.
struct EditorUiTable {
  /// The screens that read, sorted by id.
  std::vector<game::UiScreen> screens{};
  /// Every problem, each as `<id>.ui.json: path: what`, or
  /// `theme.json: what`.
  std::vector<std::string> problems{};
  /// What every screen is drawn in: `content/ui/theme.json`, or null for
  /// the dark preset when there is none, or it does not read.
  std::shared_ptr<const GuiTheme> theme{};
};

/// `content/ui/` of the project at @p root.
[[nodiscard]] std::filesystem::path
editorUiDirPath(const std::filesystem::path& root);

/// Where the screen @p id of the project at @p root is written.
[[nodiscard]] std::filesystem::path
editorUiScreenPath(const std::filesystem::path& root, std::string_view id);

/// Where the screens' theme of the project at @p root is written.
[[nodiscard]] std::filesystem::path
editorUiThemePath(const std::filesystem::path& root);

/// Whether @p id may name a screen: 1 to 64 lowercase letters, digits,
/// `_` and `-`.
[[nodiscard]] bool editorUiScreenIdValid(std::string_view id);

/// Every screen of the project at @p root, and their theme; none, and
/// the dark preset, when it has no `ui/`.
[[nodiscard]] EditorUiTable
loadEditorUiTable(const std::filesystem::path& root);

/// The screen @p id of @p table, or null when it has none.
[[nodiscard]] const game::UiScreen*
findEditorUiScreen(const EditorUiTable& table, std::string_view id);

/// The values a JSON object holds, for a screen to show: strings as they
/// are, numbers written out; anything else, or text that is no object,
/// gives none.
[[nodiscard]] game::UiValues editorUiValuesFromJson(std::string_view text);

/// Tell @p content of @p table's screens: their ids, and the actions
/// their buttons name, sorted — what a choice is numbered by.
void addEditorUiContent(const EditorUiTable& table, game::GameContent& content);

}  // namespace eng::editor

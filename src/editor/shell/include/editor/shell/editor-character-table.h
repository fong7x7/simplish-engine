#pragma once

/// @file editor-character-table.h
/// @brief The project's characters, read from their data table.
/// @par Threading Main-thread-only (touches the filesystem).

#include <filesystem>
#include <game/content/character-definition.h>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The schema a data table file declares ([project-format.md §8]).
inline constexpr std::string_view EDITOR_DATA_TABLE_SCHEMA =
    "simplish/data_table/1.0";

/// The schema every row of the characters table declares.
inline constexpr std::string_view EDITOR_CHARACTER_ENTRY_SCHEMA =
    "simplish/character/1.0";

/// Fastest a character may move, in tiles a second. Past this a player
/// crosses a screen in a blink and tunnels through thin props, since
/// collision resolves positions rather than sweeping them.
inline constexpr float EDITOR_CHARACTER_MAX_SPEED = 20.0f;

/// Most health segments a character may have: more than the HUD will ever
/// draw, and a bound on a typo.
inline constexpr float EDITOR_CHARACTER_MAX_HEALTH = 99.0f;

/// The characters table as the editor read it.
/// @thread_safety Main-thread-only.
struct EditorCharacterTable {
  /// Every character the file defines that could be read, in file order.
  std::vector<game::CharacterDefinition> characters{};
  /// What was wrong with the file, one line each, for the log and the
  /// agent API: a row skipped, a stat held to its range. Empty for a
  /// clean file and for a project with no table at all.
  std::vector<std::string> problems{};
};

/// Where a project keeps its characters:
/// `<root>/content/data/characters.data.json`.
[[nodiscard]] std::filesystem::path
editorCharacterTablePath(const std::filesystem::path& root);

/// The characters in the data table @p text.
///
/// Forgiving, as the level reader is, because the file is written by hand:
/// a row with no usable id, or one whose id an earlier row took, is skipped
/// and said so; a missing stat takes the default character's; a stat out
/// of range is held to it and said so. A file that is not a characters
/// table at all gives no characters and one problem.
[[nodiscard]] EditorCharacterTable
parseEditorCharacterTable(std::string_view text);

/// The characters table under @p root. No characters and no problems when
/// the project has no table, which is most projects.
[[nodiscard]] EditorCharacterTable
loadEditorCharacterTable(const std::filesystem::path& root);

}  // namespace eng::editor

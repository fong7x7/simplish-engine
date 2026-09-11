#pragma once

/// @file editor-enemy-table.h
/// @brief The project's enemy archetypes, read from their data table.
/// @par Threading Main-thread-only (touches the filesystem).

#include <filesystem>
#include <game/content/enemy-definition.h>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The schema every row of the enemies table declares.
inline constexpr std::string_view EDITOR_ENEMY_ENTRY_SCHEMA =
    "simplish/enemy/1.0";

/// The most health segments an archetype may have.
inline constexpr float EDITOR_ENEMY_MAX_HEALTH = 999.0F;

/// The widest body an archetype may have, in tiles of radius: a boss four
/// tiles across.
inline constexpr float EDITOR_ENEMY_MAX_RADIUS = 2.0F;

/// The tallest body an archetype may have, in tiles.
inline constexpr float EDITOR_ENEMY_MAX_HEIGHT = 8.0F;

/// The enemies table as the editor read it.
/// @thread_safety Main-thread-only.
struct EditorEnemyTable {
  /// Every archetype the file defines that could be read, in file order.
  std::vector<game::EnemyDefinition> enemies{};
  /// What was wrong with the file, one line each, for the log and the agent
  /// API. Empty for a clean file and for a project with no table.
  std::vector<std::string> problems{};
};

/// Where a project keeps its enemy archetypes:
/// `<root>/content/data/enemies.data.json`.
[[nodiscard]] std::filesystem::path
editorEnemyTablePath(const std::filesystem::path& root);

/// The archetypes in the data table @p text (project-format §8.3).
///
/// Forgiving, as the characters reader is: a row with no usable id, or one
/// an earlier row took, is skipped; a stat that is not a number takes its
/// default and one out of range is held to it; a faction the format does
/// not know is hostile; a row naming no behavior stands idle. A behavior is
/// named by id or by `behavior:` reference and kept as an id whether or not
/// anything defines it — the behaviors are read separately, and may be
/// fixed. Each of those is said in `problems`.
[[nodiscard]] EditorEnemyTable parseEditorEnemyTable(std::string_view text);

/// The enemies table under @p root. No archetypes and no problems when the
/// project has no table, which is most projects.
[[nodiscard]] EditorEnemyTable
loadEditorEnemyTable(const std::filesystem::path& root);

}  // namespace eng::editor

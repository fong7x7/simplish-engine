#include <algorithm>
#include <cmath>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-enemy-table.h>
#include <editor/shell/editor-entity-id.h>
#include <game/content/behavior-names.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <system_error>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// One string under @p key, or empty when it is absent or is not one.
  std::string readString(const json& entry, const char* key) {
    const auto found = entry.find(key);
    return found != entry.end() && found->is_string()
               ? found->get<std::string>()
               : std::string{};
  }

  /// A row being read: its id, for problem lines, and where they go.
  struct RowRead {
    /// The row's id.
    const std::string& id;
    /// Where problems go.
    std::vector<std::string>& problems;
  };

  /// The range a stat is held to, and what it is when a row says nothing.
  struct StatRange {
    /// Its default.
    float fallback = 0.0F;
    /// The least it may be.
    float low = 0.0F;
    /// The most it may be.
    float high = 0.0F;
  };

  /// The number under @p key, held to @p range: its default when it is
  /// absent or not a number, and noted in @p row when it was either of the
  /// last two or outside the range.
  float readStat(const json& entry, const char* key, StatRange range,
                 const RowRead& row) {
    const auto found = entry.find(key);
    if (found == entry.end()) {
      return range.fallback;
    }
    if (!found->is_number()) {
      row.problems.push_back(row.id + ": " + key +
                             " is not a number, so it is the default");
      return range.fallback;
    }
    const float value = found->get<float>();
    const float held = std::clamp(value, range.low, range.high);
    if (held != value) {
      row.problems.push_back(row.id + ": " + key + " was held to " +
                             std::to_string(held));
    }
    return held;
  }

  /// The behavior id row @p entry names — by id or by reference — or
  /// `idle`, noted in @p row, when it names none.
  std::string readBehavior(const json& entry, const RowRead& row) {
    const std::string named = readString(entry, "behavior");
    std::string id = named.contains(':') ? editorBehaviorIdOf(named) : named;
    if (id.empty()) {
      row.problems.push_back(row.id + ": no behavior, so it stands idle");
      return "idle";
    }
    return id;
  }

  /// The side row @p entry is on: hostile, noted in @p row, for a word the
  /// format does not know.
  game::Faction readFaction(const json& entry, const RowRead& row) {
    const std::string word = readString(entry, "faction");
    const auto faction = game::parseFaction(word);
    if (!faction && !word.empty()) {
      row.problems.push_back(row.id + ": \"" + word +
                             "\" is not a faction, so it is hostile");
    }
    return faction.value_or(game::Faction::HOSTILE);
  }

  /// The body and health of row @p entry into @p enemy.
  void readBody(const json& entry, game::EnemyDefinition& enemy,
                const RowRead& row) {
    enemy.health = static_cast<uint16_t>(
        std::lround(readStat(entry, "health",
                             {static_cast<float>(game::ENEMY_DEFAULT_HEALTH),
                              1.0F, EDITOR_ENEMY_MAX_HEALTH},
                             row)));
    enemy.radius = readStat(
        entry, "radius",
        {game::ENEMY_DEFAULT_RADIUS_TILES, 0.05F, EDITOR_ENEMY_MAX_RADIUS},
        row);
    enemy.height = readStat(
        entry, "height",
        {game::ENEMY_DEFAULT_HEIGHT_TILES, 0.1F, EDITOR_ENEMY_MAX_HEIGHT}, row);
  }

  /// The blast row @p entry goes off in when it dies, into @p enemy: a
  /// `death_blast_radius` of 0 — the default — is none.
  void readDeathBlast(const json& entry, game::EnemyDefinition& enemy,
                      const RowRead& row) {
    enemy.death_blast_radius = readStat(
        entry, "death_blast_radius", {0.0F, 0.0F, EDITOR_ENEMY_MAX_BLAST}, row);
    enemy.death_blast_damage = static_cast<uint16_t>(
        std::lround(readStat(entry, "death_blast_damage",
                             {0.0F, 0.0F, EDITOR_ENEMY_MAX_HEALTH}, row)));
  }

  /// Whether @p id can name a new row of @p table; when it cannot, the
  /// reason is added to the table's problems.
  bool usableId(const std::string& id, EditorEnemyTable& table) {
    if (id.empty() || makeEditorIdentifier(id) != id) {
      table.problems.emplace_back("a row whose id is missing or not an id "
                                  "(lowercase, digits and underscores) was "
                                  "skipped");
      return false;
    }
    if (std::ranges::any_of(table.enemies, [&id](const auto& enemy) {
          return enemy.id == id;
        })) {
      table.problems.emplace_back(id +
                                  ": a second row with this id was skipped");
      return false;
    }
    return true;
  }

  /// One row, or nothing — said why in @p table — when it cannot be used.
  std::optional<game::EnemyDefinition> readRow(const json& entry,
                                               EditorEnemyTable& table) {
    const std::string id = entry.is_object() ? readString(entry, "id") : "";
    if (!usableId(id, table)) {
      return std::nullopt;
    }
    const RowRead row{id, table.problems};
    game::EnemyDefinition enemy{.id = id,
                                .name = readString(entry, "name"),
                                .model = readString(entry, "model")};
    if (enemy.name.empty()) {
      enemy.name = id;
    }
    readBody(entry, enemy, row);
    readDeathBlast(entry, enemy, row);
    enemy.behavior = readBehavior(entry, row);
    enemy.faction = readFaction(entry, row);
    return enemy;
  }

  /// The `entries` array of @p file, or nothing when it is not an enemies
  /// table.
  std::optional<json> entriesOf(const json& file) {
    if (!file.is_object() ||
        file.value("schema", std::string{}) != EDITOR_DATA_TABLE_SCHEMA) {
      return std::nullopt;
    }
    const json content = file.value("content", json::object());
    if (!content.is_object() || content.value("entry_schema", std::string{}) !=
                                    EDITOR_ENEMY_ENTRY_SCHEMA) {
      return std::nullopt;
    }
    const json entries = content.value("entries", json::array());
    return entries.is_array() ? std::optional{entries} : std::nullopt;
  }

  /// The problem a file that is not an enemies table has.
  std::string notATable() {
    return "not an enemies table: it should be valid JSON with schema \"" +
           std::string(EDITOR_DATA_TABLE_SCHEMA) + "\" and entry_schema \"" +
           std::string(EDITOR_ENEMY_ENTRY_SCHEMA) + "\"";
  }

}  // namespace

std::filesystem::path editorEnemyTablePath(const std::filesystem::path& root) {
  return projectContentPath(root) / "data" / "enemies.data.json";
}

EditorEnemyTable parseEditorEnemyTable(std::string_view text) {
  EditorEnemyTable table;
  const json file = json::parse(std::string(text), nullptr, false);
  const std::optional<json> entries =
      file.is_discarded() ? std::nullopt : entriesOf(file);
  if (!entries) {
    table.problems.push_back(notATable());
    return table;
  }
  for (const json& entry : *entries) {
    if (std::optional<game::EnemyDefinition> row = readRow(entry, table)) {
      table.enemies.push_back(std::move(*row));
    }
  }
  return table;
}

EditorEnemyTable loadEditorEnemyTable(const std::filesystem::path& root) {
  const std::filesystem::path path = editorEnemyTablePath(root);
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return {};
  }
  const std::optional<std::string> text = readProjectTextFile(path);
  if (!text) {
    return {{}, {"could not read " + path.string()}};
  }
  return parseEditorEnemyTable(*text);
}

}  // namespace eng::editor

#include <algorithm>
#include <cmath>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-entity-id.h>
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

  /// Where a stat being read notes what was wrong with it.
  struct StatRead {
    /// Row the stat belongs to, for the problem line.
    const std::string& id;
    /// Where problems go.
    std::vector<std::string>& problems;
  };

  /// The number under @p key, or @p fallback when it is absent — or when
  /// it is not a number, which is noted in @p context.
  float readStat(const json& entry, const char* key, float fallback,
                 const StatRead& context) {
    const auto found = entry.find(key);
    if (found == entry.end()) {
      return fallback;
    }
    if (!found->is_number()) {
      context.problems.emplace_back(context.id + ": " + key +
                                    " is not a number, so it is the default");
      return fallback;
    }
    return found->get<float>();
  }

  /// @p value held to [@p low, @p high], noting in @p context when it was
  /// outside them.
  float clampStat(float value, float low, float high, const StatRead& context) {
    const float held = std::clamp(value, low, high);
    if (held != value) {
      context.problems.emplace_back(context.id + ": a stat was held to " +
                                    std::to_string(held));
    }
    return held;
  }

  /// The stats of row @p entry into @p character.
  void readStats(const json& entry, game::CharacterDefinition& character,
                 std::vector<std::string>& problems) {
    const StatRead context{character.id, problems};
    character.move_speed =
        clampStat(readStat(entry, "move_speed",
                           game::DEFAULT_CHARACTER_MOVE_SPEED, context),
                  0.0f, EDITOR_CHARACTER_MAX_SPEED, context);
    const float health = clampStat(
        readStat(entry, "health",
                 static_cast<float>(game::DEFAULT_CHARACTER_HEALTH), context),
        1.0f, EDITOR_CHARACTER_MAX_HEALTH, context);
    character.health = static_cast<uint16_t>(std::lround(health));
  }

  /// Whether @p characters already has one called @p id.
  bool taken(const std::vector<game::CharacterDefinition>& characters,
             const std::string& id) {
    return std::ranges::any_of(
        characters,
        [&id](const game::CharacterDefinition& c) { return c.id == id; });
  }

  /// Whether @p id can name a new row of @p table; when it cannot, the
  /// reason is added to the table's problems.
  bool usableId(const std::string& id, EditorCharacterTable& table) {
    if (id.empty() || makeEditorIdentifier(id) != id) {
      table.problems.emplace_back("a row whose id is missing or not an id "
                                  "(lowercase, digits and underscores) was "
                                  "skipped");
      return false;
    }
    if (taken(table.characters, id)) {
      table.problems.emplace_back(id +
                                  ": a second row with this id was skipped");
      return false;
    }
    return true;
  }

  /// One row, or nothing — said why in @p table — when it cannot be used.
  std::optional<game::CharacterDefinition>
  readRow(const json& entry, EditorCharacterTable& table) {
    const std::string id = entry.is_object() ? readString(entry, "id") : "";
    if (!usableId(id, table)) {
      return std::nullopt;
    }
    game::CharacterDefinition character;
    character.id = id;
    character.name = readString(entry, "name");
    character.model = readString(entry, "model");
    if (character.name.empty()) {
      character.name = id;
    }
    readStats(entry, character, table.problems);
    return character;
  }

  /// The `entries` array of @p file, or nothing when it is not a
  /// characters table.
  std::optional<json> entriesOf(const json& file) {
    if (!file.is_object() ||
        file.value("schema", std::string{}) != EDITOR_DATA_TABLE_SCHEMA) {
      return std::nullopt;
    }
    const json content = file.value("content", json::object());
    if (!content.is_object() || content.value("entry_schema", std::string{}) !=
                                    EDITOR_CHARACTER_ENTRY_SCHEMA) {
      return std::nullopt;
    }
    const json entries = content.value("entries", json::array());
    return entries.is_array() ? std::optional{entries} : std::nullopt;
  }

  /// The problem a file that is not a characters table has.
  std::string notATable() {
    return "not a characters table: it should be valid JSON with schema \"" +
           std::string(EDITOR_DATA_TABLE_SCHEMA) + "\" and entry_schema \"" +
           std::string(EDITOR_CHARACTER_ENTRY_SCHEMA) + "\"";
  }

}  // namespace

std::filesystem::path
editorCharacterTablePath(const std::filesystem::path& root) {
  return projectContentPath(root) / "data" / "characters.data.json";
}

EditorCharacterTable parseEditorCharacterTable(std::string_view text) {
  EditorCharacterTable table;
  const json file = json::parse(std::string(text), nullptr, false);
  const std::optional<json> entries =
      file.is_discarded() ? std::nullopt : entriesOf(file);
  if (!entries) {
    table.problems.emplace_back(notATable());
    return table;
  }
  for (const json& entry : *entries) {
    if (std::optional<game::CharacterDefinition> row = readRow(entry, table)) {
      table.characters.push_back(std::move(*row));
    }
  }
  return table;
}

EditorCharacterTable
loadEditorCharacterTable(const std::filesystem::path& root) {
  const std::filesystem::path path = editorCharacterTablePath(root);
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return {};
  }
  const std::optional<std::string> text = readProjectTextFile(path);
  if (!text) {
    return {{}, {"could not read " + path.string()}};
  }
  return parseEditorCharacterTable(*text);
}

}  // namespace eng::editor

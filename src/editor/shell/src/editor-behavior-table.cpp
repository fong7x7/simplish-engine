#include "editor-behavior-row.h"

#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-entity-id.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <system_error>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// Whether @p behaviors already has one called @p id.
  bool taken(const std::vector<game::BehaviorDefinition>& behaviors,
             const std::string& id) {
    return std::ranges::any_of(
        behaviors,
        [&id](const game::BehaviorDefinition& b) { return b.id == id; });
  }

  /// Whether @p id can name a new row of @p table; when it cannot, the
  /// reason is added to the table's problems.
  bool usableId(const std::string& id, EditorBehaviorTable& table) {
    if (id.empty() || makeEditorIdentifier(id) != id) {
      table.problems.emplace_back("a row whose id is missing or not an id "
                                  "(lowercase, digits and underscores) was "
                                  "skipped");
      return false;
    }
    if (taken(table.behaviors, id)) {
      table.problems.emplace_back(id +
                                  ": a second row with this id was skipped");
      return false;
    }
    return true;
  }

  /// The `entries` array of @p file, or nothing when it is not a behaviors
  /// table.
  std::optional<json> entriesOf(const json& file) {
    if (!file.is_object() ||
        file.value("schema", std::string{}) != EDITOR_DATA_TABLE_SCHEMA) {
      return std::nullopt;
    }
    const json content = file.value("content", json::object());
    if (!content.is_object() || content.value("entry_schema", std::string{}) !=
                                    EDITOR_BEHAVIOR_ENTRY_SCHEMA) {
      return std::nullopt;
    }
    const json entries = content.value("entries", json::array());
    return entries.is_array() ? std::optional{entries} : std::nullopt;
  }

  /// The problem a file that is not a behaviors table has.
  std::string notATable() {
    return "not a behaviors table: it should be valid JSON with schema \"" +
           std::string(EDITOR_DATA_TABLE_SCHEMA) + "\" and entry_schema \"" +
           std::string(EDITOR_BEHAVIOR_ENTRY_SCHEMA) + "\"";
  }

  /// Read one row of @p entries into @p table, if it can be.
  void readRow(const json& entry, EditorBehaviorTable& table) {
    const std::string id =
        entry.is_object() ? entry.value("id", std::string{}) : std::string{};
    if (!usableId(id, table)) {
      return;
    }
    if (auto row = readEditorBehaviorRow(entry, id, table.problems)) {
      table.behaviors.push_back(std::move(*row));
    }
  }

}  // namespace

std::filesystem::path
editorBehaviorTablePath(const std::filesystem::path& root) {
  return projectContentPath(root) / "data" / "behaviors.data.json";
}

EditorBehaviorTable parseEditorBehaviorTable(std::string_view text) {
  EditorBehaviorTable table;
  const json file = json::parse(std::string(text), nullptr, false);
  const std::optional<json> entries =
      file.is_discarded() ? std::nullopt : entriesOf(file);
  if (!entries) {
    table.problems.emplace_back(notATable());
    return table;
  }
  for (const json& entry : *entries) {
    readRow(entry, table);
  }
  return table;
}

EditorBehaviorTable loadEditorBehaviorTable(const std::filesystem::path& root) {
  const std::filesystem::path path = editorBehaviorTablePath(root);
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return {};
  }
  const std::optional<std::string> text = readProjectTextFile(path);
  if (!text) {
    return {{}, {"could not read " + path.string()}};
  }
  return parseEditorBehaviorTable(*text);
}

}  // namespace eng::editor

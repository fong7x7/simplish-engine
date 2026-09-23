#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-sound-table.h>
#include <nlohmann/json.hpp>
#include <optional>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The `entries` array of @p file, or nothing when it is not a sounds
  /// table.
  std::optional<json> entriesOf(const json& file) {
    if (!file.is_object() ||
        file.value("schema", std::string{}) != EDITOR_DATA_TABLE_SCHEMA) {
      return std::nullopt;
    }
    const json content = file.value("content", json::object());
    if (!content.is_object() || content.value("entry_schema", std::string{}) !=
                                    EDITOR_SOUND_ENTRY_SCHEMA) {
      return std::nullopt;
    }
    const json entries = content.value("entries", json::array());
    return entries.is_array() ? std::optional{entries} : std::nullopt;
  }

  /// Whether @p table already gives @p slot a file.
  bool taken(const EditorSoundTable& table, const std::string& slot) {
    return std::ranges::any_of(table.sounds, [&slot](const auto& entry) {
      return entry.slot == slot;
    });
  }

  /// Add row @p row to @p table, or say why it was skipped.
  void readRow(const json& row, EditorSoundTable& table) {
    const std::string slot =
        row.is_object() ? row.value("id", std::string{}) : std::string{};
    const std::string file =
        row.is_object() ? row.value("file", std::string{}) : std::string{};
    if (slot.empty() || file.empty()) {
      table.problems.emplace_back("a row with no id or no file was skipped");
    } else if (taken(table, slot)) {
      table.problems.push_back(slot +
                               ": a second row for this sound was skipped");
    } else {
      table.sounds.push_back({slot, std::filesystem::path(file)});
    }
  }

}  // namespace

std::filesystem::path editorSoundTablePath(const std::filesystem::path& root) {
  return projectContentPath(root) / "data" / "sounds.data.json";
}

EditorSoundTable parseEditorSoundTable(std::string_view text) {
  EditorSoundTable table;
  const json file = json::parse(std::string(text), nullptr, false);
  const std::optional<json> entries = entriesOf(file);
  if (!entries) {
    table.problems.emplace_back("not a sounds table (" +
                                std::string{EDITOR_SOUND_ENTRY_SCHEMA} + ")");
    return table;
  }
  for (const json& row : *entries) {
    readRow(row, table);
  }
  return table;
}

EditorSoundTable loadEditorSoundTable(const std::filesystem::path& root) {
  std::error_code error;
  const std::filesystem::path path = editorSoundTablePath(root);
  if (!std::filesystem::exists(path, error)) {
    return {};
  }
  if (const std::optional<std::string> text = readProjectTextFile(path)) {
    return parseEditorSoundTable(*text);
  }
  return {.problems = {"sounds.data.json could not be read"}};
}

std::string writeEditorSoundTable(const EditorSoundTable& table) {
  nlohmann::ordered_json entries = nlohmann::ordered_json::array();
  for (const EditorSoundEntry& entry : table.sounds) {
    entries.push_back(
        {{"id", entry.slot}, {"file", entry.file.generic_string()}});
  }
  const nlohmann::ordered_json file{
      {"schema", EDITOR_DATA_TABLE_SCHEMA},
      {"id", "sounds"},
      {"name", "Sounds"},
      {"content",
       {{"entry_schema", EDITOR_SOUND_ENTRY_SCHEMA}, {"entries", entries}}}};
  return file.dump(2) + "\n";
}

bool saveEditorSoundTable(const std::filesystem::path& root,
                          const EditorSoundTable& table) {
  return writeProjectTextFile(editorSoundTablePath(root),
                              writeEditorSoundTable(table));
}

}  // namespace eng::editor

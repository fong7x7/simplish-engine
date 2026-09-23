#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-animation-event-table.h>
#include <editor/shell/editor-character-table.h>
#include <nlohmann/json.hpp>
#include <optional>

namespace eng::editor {

namespace {

  using nlohmann::json;
  using nlohmann::ordered_json;

  /// The loudest an event may be, as a multiple of its recording.
  constexpr float MAX_GAIN = 4.0F;

  /// The `entries` array of @p file, or nothing when it is not this table.
  std::optional<json> entriesOf(const json& file) {
    if (!file.is_object() ||
        file.value("schema", std::string{}) != EDITOR_DATA_TABLE_SCHEMA) {
      return std::nullopt;
    }
    const json content = file.value("content", json::object());
    if (!content.is_object() || content.value("entry_schema", std::string{}) !=
                                    EDITOR_ANIMATION_EVENT_SCHEMA) {
      return std::nullopt;
    }
    const json entries = content.value("entries", json::array());
    return entries.is_array() ? std::optional{entries} : std::nullopt;
  }

  /// The number at @p key of @p item, or @p fallback.
  float numberAt(const json& item, const char* key, float fallback) {
    const auto found = item.find(key);
    return found != item.end() && found->is_number() ? found->get<float>()
                                                     : fallback;
  }

  /// The events of row @p row, each read by @p read, skipping — and noting
  /// in @p problems — any with no sound.
  template <typename Event, typename Read>
  std::vector<Event> readEvents(const json& row, Read read,
                                std::vector<std::string>& problems) {
    std::vector<Event> events;
    for (const json& item : row.value("events", json::array())) {
      const std::string sound =
          item.is_object() ? item.value("sound", std::string{}) : std::string{};
      if (sound.empty()) {
        problems.emplace_back("an event with no sound was skipped");
        continue;
      }
      events.push_back(read(item, sound));
    }
    return events;
  }

  /// One clip's event, from @p item.
  EditorAnimationEvent clipEvent(const json& item, const std::string& sound) {
    return {std::max(0.0F, numberAt(item, "at", 0.0F)), sound,
            std::clamp(numberAt(item, "gain", 1.0F), 0.0F, MAX_GAIN)};
  }

  /// One sheet's event, from @p item.
  EditorFrameEvent frameEvent(const json& item, const std::string& sound) {
    const float frame =
        std::clamp(numberAt(item, "frame", 0.0F), 0.0F, 4095.0F);
    return {static_cast<uint16_t>(frame), sound,
            std::clamp(numberAt(item, "gain", 1.0F), 0.0F, MAX_GAIN)};
  }

  /// Add clip row @p row, naming @p asset and @p clip, to @p table.
  void readClipRow(const json& row, const std::string& asset,
                   const std::string& clip, EditorAnimationEventTable& table) {
    const bool taken = std::ranges::any_of(table.clips, [&](const auto& e) {
      return e.asset == asset && e.clip == clip;
    });
    if (taken) {
      table.problems.push_back(asset + " " + clip +
                               ": a second row for this clip was skipped");
      return;
    }
    table.clips.push_back(
        {asset, clip,
         readEvents<EditorAnimationEvent>(row, clipEvent, table.problems)});
  }

  /// Add sheet row @p row, naming @p sheet, to @p table.
  void readSheetRow(const json& row, const std::string& sheet,
                    EditorAnimationEventTable& table) {
    if (std::ranges::find(table.sheets, sheet, &EditorSheetEventEntry::sheet) !=
        table.sheets.end()) {
      table.problems.push_back(sheet + ": a second row for this sheet was "
                                       "skipped");
      return;
    }
    table.sheets.push_back(
        {sheet, readEvents<EditorFrameEvent>(row, frameEvent, table.problems)});
  }

  /// Add row @p row to @p table, or say why it was skipped.
  void readRow(const json& row, EditorAnimationEventTable& table) {
    const auto text = [&row](const char* key) {
      return row.is_object() ? row.value(key, std::string{}) : std::string{};
    };
    if (!text("asset").empty() && !text("clip").empty()) {
      readClipRow(row, text("asset"), text("clip"), table);
    } else if (!text("sheet").empty()) {
      readSheetRow(row, text("sheet"), table);
    } else {
      table.problems.emplace_back(
          "a row naming neither an asset and a clip nor a sheet was skipped");
    }
  }

  /// @p entry as a row of the file.
  ordered_json clipRow(const EditorClipEventEntry& entry) {
    ordered_json events = ordered_json::array();
    for (const EditorAnimationEvent& event : entry.events) {
      events.push_back(
          {{"at", event.at}, {"sound", event.sound}, {"gain", event.gain}});
    }
    return {{"id", entry.asset + "." + entry.clip},
            {"asset", entry.asset},
            {"clip", entry.clip},
            {"events", events}};
  }

  /// @p entry as a row of the file.
  ordered_json sheetRow(const EditorSheetEventEntry& entry) {
    ordered_json events = ordered_json::array();
    for (const EditorFrameEvent& event : entry.events) {
      events.push_back({{"frame", event.frame},
                        {"sound", event.sound},
                        {"gain", event.gain}});
    }
    return {{"id", entry.sheet}, {"sheet", entry.sheet}, {"events", events}};
  }

}  // namespace

std::filesystem::path
editorAnimationEventTablePath(const std::filesystem::path& root) {
  return projectContentPath(root) / "data" / "animation-events.data.json";
}

EditorAnimationEventTable
parseEditorAnimationEventTable(std::string_view text) {
  EditorAnimationEventTable table;
  const json file = json::parse(std::string(text), nullptr, false);
  const std::optional<json> entries = entriesOf(file);
  if (!entries) {
    table.problems.push_back("not an animation events table (" +
                             std::string{EDITOR_ANIMATION_EVENT_SCHEMA} + ")");
    return table;
  }
  for (const json& row : *entries) {
    readRow(row, table);
  }
  return table;
}

EditorAnimationEventTable
loadEditorAnimationEventTable(const std::filesystem::path& root) {
  std::error_code error;
  const std::filesystem::path path = editorAnimationEventTablePath(root);
  if (!std::filesystem::exists(path, error)) {
    return {};
  }
  if (const std::optional<std::string> text = readProjectTextFile(path)) {
    return parseEditorAnimationEventTable(*text);
  }
  return {.problems = {"animation-events.data.json could not be read"}};
}

std::string
writeEditorAnimationEventTable(const EditorAnimationEventTable& table) {
  ordered_json entries = ordered_json::array();
  for (const EditorClipEventEntry& entry : table.clips) {
    entries.push_back(clipRow(entry));
  }
  for (const EditorSheetEventEntry& entry : table.sheets) {
    entries.push_back(sheetRow(entry));
  }
  const ordered_json file{{"schema", EDITOR_DATA_TABLE_SCHEMA},
                          {"id", "animation_events"},
                          {"name", "Animation events"},
                          {"content",
                           {{"entry_schema", EDITOR_ANIMATION_EVENT_SCHEMA},
                            {"entries", entries}}}};
  return file.dump(2) + "\n";
}

bool saveEditorAnimationEventTable(const std::filesystem::path& root,
                                   const EditorAnimationEventTable& table) {
  return writeProjectTextFile(editorAnimationEventTablePath(root),
                              writeEditorAnimationEventTable(table));
}

}  // namespace eng::editor

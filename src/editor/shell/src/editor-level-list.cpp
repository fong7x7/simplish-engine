#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-list.h>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace eng::editor {

namespace {

  /// `<id>.level.json` → `<id>`, and nothing for a name that is not one.
  std::optional<std::string>
  levelIdOfFile(const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    if (!entry.is_regular_file(ec) || ec) {
      return std::nullopt;
    }
    const std::string name = entry.path().filename().string();
    if (!name.ends_with(EDITOR_LEVEL_FILE_SUFFIX) ||
        name.size() == EDITOR_LEVEL_FILE_SUFFIX.size()) {
      return std::nullopt;
    }
    return name.substr(0, name.size() - EDITOR_LEVEL_FILE_SUFFIX.size());
  }

  /// Every level file directly under @p dir, unsorted.
  std::vector<EditorLevelEntry> levelFilesIn(const std::filesystem::path& dir) {
    std::vector<EditorLevelEntry> found;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (std::optional<std::string> id = levelIdOfFile(entry)) {
        found.push_back({std::move(*id), true});
      }
    }
    return found;
  }

  /// Whether @p levels already lists @p id.
  bool listed(const std::vector<EditorLevelEntry>& levels,
              std::string_view id) {
    return std::any_of(levels.begin(), levels.end(),
                       [id](const EditorLevelEntry& e) { return e.id == id; });
  }

}  // namespace

std::vector<EditorLevelEntry> scanEditorLevels(const EditorShellState& state) {
  if (!state.project.loaded) {
    return {};
  }
  std::vector<EditorLevelEntry> levels =
      levelFilesIn(projectLevelsPath(state.project.root));
  // The open level belongs in the list even with no file behind it yet:
  // it is what the menu marks as current, and leaving it out would show a
  // new project as having no level at all.
  if (!listed(levels, state.level_id)) {
    levels.push_back({state.level_id, false});
  }
  std::sort(levels.begin(), levels.end(),
            [](const EditorLevelEntry& a, const EditorLevelEntry& b) {
              return a.id < b.id;
            });
  return levels;
}

void refreshEditorLevels(EditorShellState& state) {
  state.levels = scanEditorLevels(state);
}

bool hasEditorLevel(const EditorShellState& state, std::string_view id) {
  if (!state.project.loaded) {
    return false;
  }
  if (id == state.level_id) {
    return true;
  }
  std::error_code ec;
  const bool present = std::filesystem::is_regular_file(
      editorLevelPath(state.project.root, id), ec);
  return present && !ec;
}

}  // namespace eng::editor

#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-json.h>
#include <optional>
#include <string>
#include <system_error>

namespace eng::editor {

namespace {

  /// File name of the one level a project has, as the format spells it:
  /// `<id>.level.json` ([project-format.md §4]).
  std::string levelFileName() {
    return std::string(EDITOR_LEVEL_ID) + ".level.json";
  }

}  // namespace

std::filesystem::path editorLevelPath(const std::filesystem::path& root) {
  return projectLevelsPath(root) / levelFileName();
}

bool editorLevelExists(const EditorShellState& state) {
  if (!state.project.loaded) {
    return false;
  }
  std::error_code ec;
  const bool present =
      std::filesystem::is_regular_file(editorLevelPath(state.project.root), ec);
  return present && !ec;
}

bool saveEditorLevel(const EditorShellState& state) {
  if (!state.project.loaded) {
    return false;
  }
  return writeProjectTextFile(
      editorLevelPath(state.project.root),
      serializeEditorLevel(state.document, state.assets,
                           state.project.metadata.name));
}

std::optional<EditorLevelLoad> loadEditorLevel(const EditorShellState& state) {
  if (!state.project.loaded) {
    return std::nullopt;
  }
  const std::optional<std::string> text =
      readProjectTextFile(editorLevelPath(state.project.root));
  if (!text) {
    return std::nullopt;
  }
  return parseEditorLevel(*text, state.assets);
}

}  // namespace eng::editor

#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-json.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace eng::editor {

std::filesystem::path editorLevelPath(const std::filesystem::path& root,
                                      std::string_view id) {
  return projectLevelsPath(root) /
         (std::string(id) + std::string(EDITOR_LEVEL_FILE_SUFFIX));
}

std::filesystem::path editorLevelPath(const EditorShellState& state) {
  return editorLevelPath(state.project.root, state.level_id);
}

bool editorLevelExists(const EditorShellState& state) {
  if (!state.project.loaded) {
    return false;
  }
  std::error_code ec;
  const bool present =
      std::filesystem::is_regular_file(editorLevelPath(state), ec);
  return present && !ec;
}

bool saveEditorLevel(const EditorShellState& state) {
  if (!state.project.loaded) {
    return false;
  }
  return writeProjectTextFile(
      editorLevelPath(state),
      serializeEditorLevel(state.document, state.assets, state.level_id));
}

bool createEditorLevelFile(const std::filesystem::path& root,
                           std::string_view id) {
  // An empty asset list, because an empty level names no asset: the props
  // that would need one are exactly what this file does not have yet.
  return writeProjectTextFile(editorLevelPath(root, id),
                              serializeEditorLevel({}, {}, id));
}

std::optional<EditorLevelLoad> loadEditorLevel(const EditorShellState& state) {
  if (!state.project.loaded) {
    return std::nullopt;
  }
  const std::optional<std::string> text =
      readProjectTextFile(editorLevelPath(state));
  if (!text) {
    return std::nullopt;
  }
  return parseEditorLevel(*text, state.assets);
}

}  // namespace eng::editor

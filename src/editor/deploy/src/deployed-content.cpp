#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-content.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-enemy-table.h>
#include <editor/shell/editor-ui-table.h>

namespace eng::editor {

std::optional<game::GameSetup>
readDeployedSetup(const std::filesystem::path& content,
                  const std::string& level) {
  const std::optional<std::string> text =
      readProjectTextFile(content / EDITOR_DEPLOY_LEVELS_DIR /
                          (level + std::string(EDITOR_SETUP_FILE_SUFFIX)));
  return text ? parseGameSetup(*text) : std::nullopt;
}

game::GameContent readDeployedContent(const std::filesystem::path& content) {
  game::GameContent read{loadEditorCharacterTable(content).characters,
                         loadEditorBehaviorTable(content).behaviors,
                         loadEditorEnemyTable(content).enemies};
  addEditorUiContent(loadEditorUiTable(content), read);
  return read;
}

}  // namespace eng::editor

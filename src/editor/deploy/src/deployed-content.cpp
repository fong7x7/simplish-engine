#include <algorithm>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-file-hash.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-content.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-enemy-table.h>
#include <editor/shell/editor-ui-table.h>

namespace eng::editor {

namespace {

  /// Whether @p relative — a file's path inside the deployed game — is one
  /// the simulation reads: the manifest, a baked level, or the project's
  /// content (data tables, screens). Anything else beside the game — a
  /// replay, a desync report, the executable — is not content.
  bool isContentFile(const std::string& relative) {
    return relative == EDITOR_DEPLOY_MANIFEST ||
           relative.starts_with(std::string(EDITOR_DEPLOY_LEVELS_DIR) + "/") ||
           relative.starts_with(std::string(PROJECT_CONTENT_DIR_NAME) + "/");
  }

}  // namespace

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

uint64_t deployedContentHash(const std::filesystem::path& content) {
  return hashFileTree(content, isContentFile);
}

void makeRoomForLogic(game::GameSetup& setup,
                      const game::GameLogicInstance& logic) {
  if (logic.get() != nullptr) {
    setup.actor_capacity =
        std::max(setup.actor_capacity, game::GAME_LOGIC_ACTOR_CAPACITY);
  }
}

}  // namespace eng::editor

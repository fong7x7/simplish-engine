#include <algorithm>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-content.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-enemy-table.h>
#include <editor/shell/editor-ui-table.h>
#include <fstream>
#include <iterator>
#include <vector>

namespace eng::editor {

namespace {

  /// FNV-1a's 64-bit offset basis and prime.
  constexpr uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
  constexpr uint64_t FNV_PRIME = 0x100000001B3ULL;

  /// @p hash with @p bytes folded in.
  uint64_t fold(uint64_t hash, std::string_view bytes) {
    for (const char byte : bytes) {
      hash = (hash ^ static_cast<uint8_t>(byte)) * FNV_PRIME;
    }
    return hash;
  }

  /// Whether @p relative — a file's path inside the deployed game — is one
  /// the simulation reads: the manifest, a baked level, or the project's
  /// content (data tables, screens). Not hidden: a Finder's `.DS_Store`
  /// differs from machine to machine. Anything else beside the game — a
  /// replay, a desync report, the executable — is not content.
  bool isContentFile(const std::string& relative) {
    const bool read =
        relative == EDITOR_DEPLOY_MANIFEST ||
        relative.starts_with(std::string(EDITOR_DEPLOY_LEVELS_DIR) + "/") ||
        relative.starts_with(std::string(PROJECT_CONTENT_DIR_NAME) + "/");
    return read && !relative.contains("/.");
  }

  /// Every content file under @p content, relative to it, in path order.
  std::vector<std::string> contentFiles(const std::filesystem::path& content) {
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(content, ec)) {
      const std::string relative =
          entry.path().lexically_relative(content).generic_string();
      if (entry.is_regular_file() && isContentFile(relative)) {
        files.push_back(relative);
      }
    }
    std::ranges::sort(files);
    return files;
  }

  /// The bytes of the file at @p path; empty when it cannot be read.
  std::string fileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
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
  uint64_t hash = FNV_OFFSET;
  for (const std::string& file : contentFiles(content)) {
    // The path's terminator keeps "a" + "bc" apart from "ab" + "c".
    hash = fold(fold(hash, file), std::string_view("\0", 1));
    hash = fold(hash, fileBytes(content / file));
  }
  return hash;
}

void makeRoomForLogic(game::GameSetup& setup,
                      const game::GameLogicInstance& logic) {
  if (logic.get() != nullptr) {
    setup.actor_capacity =
        std::max(setup.actor_capacity, game::GAME_LOGIC_ACTOR_CAPACITY);
  }
}

}  // namespace eng::editor

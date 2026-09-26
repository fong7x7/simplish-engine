#include <algorithm>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-content.h>
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

  /// Whether @p path is a file the content is made of: not a folder, and
  /// not hidden — a Finder's `.DS_Store` is not content, and differs from
  /// machine to machine.
  bool isContentFile(const std::filesystem::directory_entry& entry) {
    return entry.is_regular_file() &&
           !entry.path().filename().string().starts_with('.');
  }

  /// Every content file under @p content, relative to it, in path order.
  std::vector<std::string> contentFiles(const std::filesystem::path& content) {
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(content, ec)) {
      if (isContentFile(entry)) {
        files.push_back(
            entry.path().lexically_relative(content).generic_string());
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

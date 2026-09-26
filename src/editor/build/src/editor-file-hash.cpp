#include <algorithm>
#include <editor/build/editor-file-hash.h>
#include <fstream>
#include <iterator>
#include <string_view>
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

  /// Every file under @p root that @p keep counts, relative, in path order.
  std::vector<std::string> keptFiles(const std::filesystem::path& root,
                                     EditorFileFilter keep) {
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root, ec)) {
      std::string relative =
          entry.path().lexically_relative(root).generic_string();
      if (entry.is_regular_file() && !editorPathHidden(relative) &&
          keep(relative)) {
        files.push_back(std::move(relative));
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

bool editorPathHidden(const std::string& relative) {
  return relative.starts_with('.') || relative.contains("/.");
}

uint64_t hashFileTree(const std::filesystem::path& root,
                      EditorFileFilter keep) {
  uint64_t hash = FNV_OFFSET;
  for (const std::string& file : keptFiles(root, keep)) {
    // The path's terminator keeps "a" + "bc" apart from "ab" + "c".
    hash = fold(fold(hash, file), std::string_view("\0", 1));
    hash = fold(hash, fileBytes(root / file));
  }
  return hash;
}

}  // namespace eng::editor

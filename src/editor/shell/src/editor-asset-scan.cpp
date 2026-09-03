#include <algorithm>
#include <cctype>
#include <editor/shell/editor-asset-scan.h>
#include <string>

namespace eng::editor {

namespace {

  /// Lowercase copy, so `.OBJ` lists alongside `.obj`.
  std::string toLower(std::string text) {
    for (char& c : text) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
  }

  bool isMeshFile(const std::filesystem::path& path) {
    return toLower(path.extension().string()) == ASSET_MESH_EXTENSION;
  }

  /// Add one directory entry to the list if it is a mesh file.
  void collectAsset(const std::filesystem::directory_entry& entry,
                    std::vector<EditorAsset>& out) {
    std::error_code ec;
    if (!entry.is_regular_file(ec) || !isMeshFile(entry.path())) {
      return;
    }
    out.push_back({entry.path().stem().string(), entry.path()});
  }

}  // namespace

std::vector<EditorAsset>
scanEditorAssets(const std::filesystem::path& assets_dir) {
  std::vector<EditorAsset> assets;
  std::error_code ec;
  if (!std::filesystem::is_directory(assets_dir, ec)) {
    return assets;
  }
  for (const auto& entry :
       std::filesystem::directory_iterator(assets_dir, ec)) {
    if (ec) {
      break;
    }
    collectAsset(entry, assets);
  }
  std::ranges::sort(assets, {}, &EditorAsset::name);
  return assets;
}

}  // namespace eng::editor

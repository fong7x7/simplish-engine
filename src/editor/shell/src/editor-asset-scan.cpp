#include <algorithm>
#include <cctype>
#include <editor/shell/editor-asset-scan.h>
#include <iterator>
#include <string>
#include <system_error>

namespace eng::editor {

namespace {

  namespace fs = std::filesystem;

  /// Lowercase copy, so `.OBJ` lists alongside `.obj`.
  std::string toLower(std::string text) {
    for (char& c : text) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
  }

  bool isMeshFile(const fs::path& path) {
    const std::string extension = toLower(path.extension().string());
    return std::ranges::find(ASSET_MESH_EXTENSIONS, extension) !=
           std::end(ASSET_MESH_EXTENSIONS);
  }

  /// Whether a name starts with a dot. Dot-directories hold editor and
  /// version-control metadata, and their whole subtree is skipped.
  bool isHiddenName(const fs::path& path) {
    const std::string name = path.filename().string();
    return !name.empty() && name.front() == '.';
  }

  /// Add one directory entry to the scan, as a folder or as a mesh file.
  void collectEntry(const fs::directory_entry& entry, const fs::path& root,
                    EditorAssetScan& out) {
    // Lexical, not `fs::relative`: the entry came from a walk of `root`, so
    // the answer is known without touching the filesystem again.
    const fs::path relative = entry.path().lexically_relative(root);
    std::error_code ec;
    if (entry.is_directory(ec)) {
      out.folders.push_back(relative);
      return;
    }
    if (entry.is_regular_file(ec) && isMeshFile(entry.path())) {
      out.assets.push_back(
          {entry.path().stem().string(), entry.path(), relative});
    }
  }

  /// Walk from @p it to the end, collecting entries and pruning hidden
  /// subtrees. Iteration errors stop the walk with what was found so far.
  void walk(fs::recursive_directory_iterator& it, const fs::path& root,
            EditorAssetScan& out) {
    const fs::recursive_directory_iterator end;
    std::error_code ec;
    while (it != end) {
      if (isHiddenName(it->path())) {
        it.disable_recursion_pending();
      } else {
        collectEntry(*it, root, out);
      }
      it.increment(ec);
      if (ec) {
        return;
      }
    }
  }

  /// Put the scan in relative-path order, which is stable across machines.
  void sortScan(EditorAssetScan& scan) {
    std::ranges::sort(scan.assets, {}, &EditorAsset::relative_path);
    std::ranges::sort(scan.folders);
  }

}  // namespace

bool isRiggedModelFile(const std::filesystem::path& path) {
  const std::string extension = toLower(path.extension().string());
  return extension == ".gltf" || extension == ".glb";
}

EditorAssetScan scanEditorAssets(const fs::path& assets_dir) {
  EditorAssetScan scan;
  std::error_code ec;
  if (!fs::is_directory(assets_dir, ec)) {
    return scan;
  }
  // Skipping permission-denied entries keeps one unreadable sub-directory
  // from costing the whole scan.
  fs::recursive_directory_iterator it(
      assets_dir, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    return scan;
  }
  walk(it, assets_dir, scan);
  sortScan(scan);
  return scan;
}

}  // namespace eng::editor

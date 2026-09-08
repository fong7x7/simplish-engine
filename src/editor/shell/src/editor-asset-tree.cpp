#include <algorithm>
#include <editor/shell/editor-asset-tree.h>
#include <string>
#include <unordered_map>

namespace eng::editor {

namespace {

  /// Relative path (in generic form) to the folder holding it, so a folder
  /// reached twice is created once.
  using FolderIndexMap = std::unordered_map<std::string, size_t>;

  /// Index of the folder at @p relative, creating it and any missing
  /// parents. The empty path is the root, which always exists.
  size_t ensureFolder(EditorAssetTree& tree, FolderIndexMap& index,
                      const std::filesystem::path& relative) {
    if (relative.empty()) {
      return EDITOR_ASSET_FOLDER_ROOT;
    }
    const std::string key = relative.generic_string();
    if (const auto found = index.find(key); found != index.end()) {
      return found->second;
    }
    // Resolve the parent before growing the vector: the recursion may push,
    // and an index taken beforehand would be the one that survives it.
    const size_t parent = ensureFolder(tree, index, relative.parent_path());
    const size_t self = tree.folders.size();
    tree.folders.push_back({relative.filename().string(), relative, parent});
    tree.folders[parent].child_folders.push_back(self);
    index.emplace(key, self);
    return self;
  }

  /// Add every scanned directory, so one holding nothing is still listed.
  void addFolders(EditorAssetTree& tree, FolderIndexMap& index,
                  const std::vector<std::filesystem::path>& folders) {
    for (const std::filesystem::path& folder : folders) {
      ensureFolder(tree, index, folder);
    }
  }

  /// File every asset under the folder that holds it.
  void addAssets(EditorAssetTree& tree, FolderIndexMap& index,
                 const std::vector<EditorAsset>& assets) {
    for (size_t i = 0; i < assets.size(); ++i) {
      const size_t folder =
          ensureFolder(tree, index, assets[i].relative_path.parent_path());
      tree.folders[folder].assets.push_back(i);
    }
  }

  /// Order each folder's children by name, so what the browser lists does
  /// not depend on the order the scan happened to produce them in.
  void sortChildren(EditorAssetTree& tree,
                    const std::vector<EditorAsset>& assets) {
    const auto folder_name = [&tree](size_t i) {
      return tree.folders[i].name;
    };
    const auto asset_name = [&assets](size_t i) {
      return assets[i].name;
    };
    for (EditorAssetFolder& folder : tree.folders) {
      std::ranges::sort(folder.child_folders, {}, folder_name);
      std::ranges::sort(folder.assets, {}, asset_name);
    }
  }

}  // namespace

EditorAssetTree buildEditorAssetTree(const EditorAssetScan& scan) {
  EditorAssetTree tree;
  FolderIndexMap index;
  addFolders(tree, index, scan.folders);
  addAssets(tree, index, scan.assets);
  sortChildren(tree, scan.assets);
  return tree;
}

}  // namespace eng::editor

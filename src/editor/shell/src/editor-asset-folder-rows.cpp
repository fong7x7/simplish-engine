#include <editor/shell/editor-asset-folder-rows.h>

namespace eng::editor {

namespace {

  /// What the walk carries, so the recursion stays inside the parameter
  /// budget and reads as "where am I" rather than "what am I holding".
  struct FlattenContext {
    /// Tree being flattened.
    const EditorAssetTree& tree;
    /// Folders whose children are listed.
    const std::unordered_set<size_t>& expanded;
    /// Rows produced so far, in draw order.
    std::vector<EditorAssetFolderRow>& out;
  };

  /// Emit @p folder, then its children when it is open.
  void appendRows(const FlattenContext& ctx, size_t folder, uint32_t depth) {
    const EditorAssetFolder& node = ctx.tree.folders[folder];
    const bool has_children = !node.child_folders.empty();
    const bool open = has_children && ctx.expanded.contains(folder);
    ctx.out.push_back({folder, depth, open, has_children});
    if (!open) {
      return;
    }
    for (const size_t child : node.child_folders) {
      appendRows(ctx, child, depth + 1);
    }
  }

}  // namespace

std::vector<EditorAssetFolderRow>
flattenAssetFolderRows(const EditorAssetTree& tree,
                       const std::unordered_set<size_t>& expanded) {
  std::vector<EditorAssetFolderRow> rows;
  // Every folder with no parent is a section of its own, listed at depth
  // zero in the order the tree holds them: the assets root first, and the
  // built-in General section after it.
  for (size_t folder = 0; folder < tree.folders.size(); ++folder) {
    if (tree.folders[folder].parent == EDITOR_ASSET_FOLDER_NONE) {
      appendRows({tree, expanded, rows}, folder, 0);
    }
  }
  return rows;
}

}  // namespace eng::editor

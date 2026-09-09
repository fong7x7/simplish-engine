#include <editor/shell/editor-general-section.h>
#include <string>

namespace eng::editor {

size_t appendEditorGeneralSection(EditorAssetTree& tree, size_t first_entry) {
  const size_t index = tree.folders.size();
  EditorAssetFolder folder;
  folder.name = std::string(EDITOR_GENERAL_FOLDER_NAME);
  // No parent: the pane lists it beside the assets root, not under it. It
  // has no relative path either — there is no directory it stands for.
  folder.parent = EDITOR_ASSET_FOLDER_NONE;
  for (size_t item = 0; item < EDITOR_GENERAL_ITEM_COUNT; ++item) {
    folder.assets.push_back(first_entry + item);
  }
  tree.folders.push_back(std::move(folder));
  return index;
}

}  // namespace eng::editor

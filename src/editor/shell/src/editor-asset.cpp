#include <editor/shell/editor-asset.h>

namespace eng::editor {

bool editorAssetLoaded(const EditorAsset& asset) {
  return asset.mesh != MESH_GPU_INVALID ||
         asset.skinned_mesh != MESH_GPU_INVALID;
}

}  // namespace eng::editor

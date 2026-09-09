#include <editor/shell/editor-shape.h>
#include <engine/render-mesh/mesh-primitives.h>
#include <string>

namespace eng::editor {

MeshData makeEditorShapeMesh(EditorShapeKind kind) {
  switch (kind) {
    case EditorShapeKind::CUBE:
      return makeCubeMesh();
    case EditorShapeKind::CYLINDER:
      return makeCylinderMesh();
    case EditorShapeKind::PYRAMID:
      return makePyramidMesh();
    case EditorShapeKind::SPHERE:
      return makeSphereMesh();
  }
  return {};
}

size_t appendEditorShapeAssets(std::vector<EditorAsset>& assets) {
  const size_t first = assets.size();
  for (const EditorShapeKind kind : EDITOR_SHAPE_KINDS) {
    EditorAsset asset;
    asset.name = std::string(editorShapeName(kind));
    // No path: there is no file, and the empty one is never read because
    // `shape` is what says where the geometry comes from.
    asset.shape = kind;
    assets.push_back(std::move(asset));
  }
  return first;
}

}  // namespace eng::editor

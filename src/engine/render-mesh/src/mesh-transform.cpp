#include <algorithm>
#include <engine/render-mesh/mesh-transform.h>

namespace eng {

namespace {

  /// Y-up to Z-up: (x, y, z) becomes (x, -z, y).
  Vec3 toZUp(const Vec3& v) {
    return {v.x, -v.z, v.y};
  }

  /// Recompute bounds from the vertices, which is cheaper to get right than
  /// rotating the old corners and re-deriving min and max from them.
  void refreshBounds(MeshData& mesh) {
    if (mesh.vertices.empty()) {
      return;
    }
    Vec3 min = mesh.vertices.front().position;
    Vec3 max = min;
    for (const auto& vertex : mesh.vertices) {
      min = {std::min(min.x, vertex.position.x),
             std::min(min.y, vertex.position.y),
             std::min(min.z, vertex.position.z)};
      max = {std::max(max.x, vertex.position.x),
             std::max(max.y, vertex.position.y),
             std::max(max.z, vertex.position.z)};
    }
    mesh.min = min;
    mesh.max = max;
  }

}  // namespace

void orientYUpToZUp(MeshData& mesh) {
  for (auto& vertex : mesh.vertices) {
    vertex.position = toZUp(vertex.position);
    vertex.normal = toZUp(vertex.normal);
  }
  refreshBounds(mesh);
}

}  // namespace eng

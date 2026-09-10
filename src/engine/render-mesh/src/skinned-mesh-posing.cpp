#include <algorithm>
#include <engine/math/math.h>
#include <engine/render-mesh/skinned-mesh-posing.h>
#include <limits>

namespace eng {

namespace {

  /// The weighted sum of a vertex's joint matrices: linear blend skinning,
  /// which is what the vertex shaders compute from the palette's rows.
  Mat4 blendedSkin(const SkinnedMeshVertex& vertex,
                   std::span<const Mat4> skin) {
    Mat4 blended{};
    for (size_t i = 0; i < MESH_SKIN_INFLUENCES; ++i) {
      const size_t joint = vertex.joints[i];
      const Mat4 m = joint < skin.size() ? skin[joint] : Mat4::identity();
      for (size_t e = 0; e < 16; ++e) {
        blended[e] += m[e] * vertex.weights[i];
      }
    }
    return blended;
  }

  /// @p v turned by @p m's upper 3×3 and brought back to unit length.
  Vec3 transformNormal(const Mat4& m, const Vec3& v) {
    const Vec3 turned{m(0, 0) * v.x + m(0, 1) * v.y + m(0, 2) * v.z,
                      m(1, 0) * v.x + m(1, 1) * v.y + m(1, 2) * v.z,
                      m(2, 0) * v.x + m(2, 1) * v.y + m(2, 2) * v.z};
    return Vec3::length(turned) > 0.0f ? Vec3::normalize(turned) : v;
  }

  /// Grow @p mesh's bounds to take in @p p.
  void extendBounds(MeshData& mesh, const Vec3& p) {
    mesh.min = {std::min(mesh.min.x, p.x), std::min(mesh.min.y, p.y),
                std::min(mesh.min.z, p.z)};
    mesh.max = {std::max(mesh.max.x, p.x), std::max(mesh.max.y, p.y),
                std::max(mesh.max.z, p.z)};
  }

  /// One static vertex: @p vertex where its joints carry it.
  MeshVertex posedVertex(const SkinnedMeshVertex& vertex,
                         std::span<const Mat4> skin) {
    if (skin.empty()) {
      return {vertex.position, vertex.normal, vertex.uv};
    }
    const Mat4 m = blendedSkin(vertex, skin);
    return {math::transformPoint(m, vertex.position),
            transformNormal(m, vertex.normal), vertex.uv};
  }

}  // namespace

MeshData poseSkinnedMesh(const SkinnedMeshData& mesh,
                         std::span<const Mat4> skin) {
  MeshData out;
  out.vertices.reserve(mesh.vertices.size());
  constexpr float BIG = std::numeric_limits<float>::max();
  out.min = {BIG, BIG, BIG};
  out.max = {-BIG, -BIG, -BIG};
  for (const SkinnedMeshVertex& vertex : mesh.vertices) {
    out.vertices.push_back(posedVertex(vertex, skin));
    extendBounds(out, out.vertices.back().position);
  }
  if (out.vertices.empty()) {
    out.min = {};
    out.max = {};
  }
  out.indices = mesh.indices;
  out.texture_path = mesh.texture_path;
  return out;
}

}  // namespace eng

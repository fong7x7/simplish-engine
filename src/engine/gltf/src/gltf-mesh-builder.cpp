#include "gltf-mesh-builder.h"

#include "gltf-accessor.h"
#include "gltf-uri.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <system_error>

namespace eng::gltf {

namespace {

  /// glTF's primitive mode for a triangle list, and its default.
  constexpr size_t GLTF_TRIANGLES = 4;

  /// One primitive's attributes and indices, read out of their buffers.
  struct PrimitiveStreams {
    /// Three floats per vertex.
    std::vector<float> positions;
    /// Three floats per vertex, or empty when the file has none.
    std::vector<float> normals;
    /// Two floats per vertex, or empty.
    std::vector<float> uvs;
    /// Four joint numbers per vertex, or empty.
    std::vector<uint32_t> joints;
    /// Four weights per vertex, or empty.
    std::vector<float> weights;
    /// Triangle list indices into this primitive's vertices.
    std::vector<uint32_t> indices;
  };

  /// Float attribute @p name: empty when absent, nullopt when unreadable.
  std::optional<std::vector<float>> floatAttribute(const GltfDocument& document,
                                                   const Json& attributes,
                                                   const char* name,
                                                   size_t components) {
    const auto accessor = jsonIndex(attributes, name);
    if (!accessor) {
      return std::vector<float>{};
    }
    return readAccessorFloats(document, *accessor, components);
  }

  /// The primitive's joint numbers: empty when absent, nullopt when bad.
  std::optional<std::vector<uint32_t>>
  jointAttribute(const GltfDocument& document, const Json& attributes) {
    const auto accessor = jsonIndex(attributes, "JOINTS_0");
    if (!accessor) {
      return std::vector<uint32_t>{};
    }
    return readAccessorUints(document, *accessor, 4);
  }

  /// Whether @p stream is absent or holds @p per_vertex values for each of
  /// @p count vertices.
  template <typename T>
  bool fits(const std::vector<T>& stream, size_t count, size_t per_vertex) {
    return stream.empty() || stream.size() == count * per_vertex;
  }

  /// The primitive's indices, or every vertex in order when it has none.
  /// Nullopt when one names a vertex the primitive does not have.
  std::optional<std::vector<uint32_t>>
  primitiveIndices(const GltfDocument& document, const Json& primitive,
                   size_t vertex_count) {
    const auto accessor = jsonIndex(primitive, "indices");
    if (!accessor) {
      std::vector<uint32_t> all(vertex_count);
      std::ranges::iota(all, 0U);
      return all;
    }
    auto indices = readAccessorUints(document, *accessor, 1);
    if (!indices || std::ranges::any_of(*indices, [&](uint32_t i) {
          return i >= vertex_count;
        })) {
      return std::nullopt;
    }
    return indices;
  }

  /// Whether every stream present has one entry per position.
  bool streamsAgree(const PrimitiveStreams& s) {
    const size_t count = s.positions.size() / 3;
    return fits(s.normals, count, 3) && fits(s.uvs, count, 2) &&
           fits(s.weights, count, 4) && fits(s.joints, count, 4);
  }

  /// Read every attribute this loader uses. Nullopt when one is unreadable
  /// or disagrees with the others about how many vertices there are.
  std::optional<PrimitiveStreams> readAttributes(const GltfDocument& document,
                                                 const Json& attributes) {
    auto positions = floatAttribute(document, attributes, "POSITION", 3);
    auto normals = floatAttribute(document, attributes, "NORMAL", 3);
    auto uvs = floatAttribute(document, attributes, "TEXCOORD_0", 2);
    auto weights = floatAttribute(document, attributes, "WEIGHTS_0", 4);
    auto joints = jointAttribute(document, attributes);
    if (!positions || positions->empty() || !normals || !uvs || !weights ||
        !joints) {
      return std::nullopt;
    }
    PrimitiveStreams s{std::move(*positions), std::move(*normals),
                       std::move(*uvs),       std::move(*joints),
                       std::move(*weights),   {}};
    return streamsAgree(s) ? std::optional(std::move(s)) : std::nullopt;
  }

  /// A primitive's streams, indices included.
  std::optional<PrimitiveStreams> readStreams(const GltfDocument& document,
                                              const Json& primitive) {
    const Json* attributes = jsonMember(primitive, "attributes");
    auto streams = attributes != nullptr ? readAttributes(document, *attributes)
                                         : std::nullopt;
    if (!streams) {
      return std::nullopt;
    }
    auto indices =
        primitiveIndices(document, primitive, streams->positions.size() / 3);
    if (!indices) {
      return std::nullopt;
    }
    streams->indices = std::move(*indices);
    return streams;
  }

  /// Scale @p v's weights, which sum to @p sum, to sum to one. Weights that
  /// sum to nothing hang the vertex wholly from its first joint.
  void normalizeWeights(float sum, SkinnedMeshVertex& v) {
    for (float& w : v.weights) {
      w = sum > 0.0f ? w / sum : 0.0f;
    }
    v.weights[0] = sum > 0.0f ? v.weights[0] : 1.0f;
  }

  /// Set vertex @p i's joints and weights, scaling the weights to sum to
  /// one. False when a weighted joint is outside the skin.
  bool setInfluences(const PrimitiveStreams& s, size_t i, size_t skin_joints,
                     SkinnedMeshVertex& v) {
    if (s.weights.empty() || s.joints.empty()) {
      return true;
    }
    float sum = 0.0f;
    for (size_t k = 0; k < MESH_SKIN_INFLUENCES; ++k) {
      const float w = std::max(s.weights[i * 4 + k], 0.0f);
      const uint32_t joint = s.joints[i * 4 + k];
      if (w > 0.0f && joint >= skin_joints) {
        return false;
      }
      v.joints[k] = static_cast<uint8_t>(w > 0.0f ? joint : 0U);
      v.weights[k] = w;
      sum += w;
    }
    normalizeWeights(sum, v);
    return true;
  }

  /// Vertex @p i of @p s, or nullopt when it names a joint the skin lacks.
  std::optional<SkinnedMeshVertex>
  assembleVertex(const PrimitiveStreams& s, size_t i, size_t skin_joints) {
    SkinnedMeshVertex v;
    v.position = {s.positions[i * 3], s.positions[i * 3 + 1],
                  s.positions[i * 3 + 2]};
    if (!s.normals.empty()) {
      v.normal = {s.normals[i * 3], s.normals[i * 3 + 1], s.normals[i * 3 + 2]};
    }
    if (!s.uvs.empty()) {
      v.uv = {s.uvs[i * 2], s.uvs[i * 2 + 1]};
    }
    if (!setInfluences(s, i, skin_joints, v)) {
      return std::nullopt;
    }
    return v;
  }

  /// Smooth normals for the vertices from @p first_vertex on, from the
  /// triangles from @p first_index on: each face's area-weighted normal
  /// summed into its corners, then brought to unit length.
  void computeNormals(SkinnedMeshData& mesh, size_t first_vertex,
                      size_t first_index) {
    auto& v = mesh.vertices;
    for (size_t t = first_index; t + 2 < mesh.indices.size(); t += 3) {
      const uint32_t a = mesh.indices[t];
      const uint32_t b = mesh.indices[t + 1];
      const uint32_t c = mesh.indices[t + 2];
      const Vec3 face = Vec3::cross(v[b].position - v[a].position,
                                    v[c].position - v[a].position);
      v[a].normal = v[a].normal + face;
      v[b].normal = v[b].normal + face;
      v[c].normal = v[c].normal + face;
    }
    for (size_t i = first_vertex; i < v.size(); ++i) {
      v[i].normal = Vec3::length(v[i].normal) > 0.0f
                        ? Vec3::normalize(v[i].normal)
                        : Vec3{0.0f, 1.0f, 0.0f};
    }
  }

  /// Append every vertex of @p s. False when one names a joint the skin
  /// lacks.
  bool appendVertices(const PrimitiveStreams& s, size_t skin_joints,
                      SkinnedMeshData& mesh) {
    for (size_t i = 0; i < s.positions.size() / 3; ++i) {
      const auto vertex = assembleVertex(s, i, skin_joints);
      if (!vertex) {
        return false;
      }
      mesh.vertices.push_back(*vertex);
    }
    return true;
  }

  /// Append @p indices, offset by @p base, whole triangles only: a trailing
  /// index or two draws nothing.
  void appendIndices(const std::vector<uint32_t>& indices, uint32_t base,
                     SkinnedMeshData& mesh) {
    const size_t whole = indices.size() / 3 * 3;
    for (size_t k = 0; k < whole; ++k) {
      mesh.indices.push_back(base + indices[k]);
    }
  }

  /// Append one triangle primitive to @p mesh. The first failure, or none.
  std::optional<GltfLoadError> appendPrimitive(const GltfDocument& document,
                                               const Json& primitive,
                                               size_t skin_joints,
                                               SkinnedMeshData& mesh) {
    const auto streams = readStreams(document, primitive);
    if (!streams) {
      return GltfLoadError::BAD_ACCESSOR;
    }
    const size_t base = mesh.vertices.size();
    const size_t first_index = mesh.indices.size();
    if (!appendVertices(*streams, skin_joints, mesh)) {
      return GltfLoadError::BAD_SKIN;
    }
    appendIndices(streams->indices, static_cast<uint32_t>(base), mesh);
    if (streams->normals.empty()) {
      computeNormals(mesh, base, first_index);
    }
    return std::nullopt;
  }

  /// Fit the bounds to every vertex.
  void setBounds(SkinnedMeshData& mesh) {
    constexpr float BIG = std::numeric_limits<float>::max();
    mesh.min = {BIG, BIG, BIG};
    mesh.max = {-BIG, -BIG, -BIG};
    for (const SkinnedMeshVertex& v : mesh.vertices) {
      mesh.min = {std::min(mesh.min.x, v.position.x),
                  std::min(mesh.min.y, v.position.y),
                  std::min(mesh.min.z, v.position.z)};
      mesh.max = {std::max(mesh.max.x, v.position.x),
                  std::max(mesh.max.y, v.position.y),
                  std::max(mesh.max.z, v.position.z)};
    }
  }

  /// The image the primitive's material uses as its base colour, or null —
  /// five indirections, any of which a file may leave out.
  const Json* baseColorImage(const Json& root, const Json& primitive) {
    const auto material = jsonIndex(primitive, "material");
    const Json* m =
        material ? jsonElement(root, "materials", *material) : nullptr;
    const Json* pbr = m ? jsonMember(*m, "pbrMetallicRoughness") : nullptr;
    const Json* slot = pbr ? jsonMember(*pbr, "baseColorTexture") : nullptr;
    const auto texture = slot ? jsonIndex(*slot, "index") : std::nullopt;
    const Json* t = texture ? jsonElement(root, "textures", *texture) : nullptr;
    const auto source = t ? jsonIndex(*t, "source") : std::nullopt;
    return source ? jsonElement(root, "images", *source) : nullptr;
  }

  /// The base colour image as a file beside the document, when it is one
  /// and it is there. Images inside a buffer or a data URI are not read.
  std::filesystem::path texturePath(const GltfDocument& document,
                                    const Json& primitive) {
    const Json* image = baseColorImage(document.root, primitive);
    const std::string uri = image ? jsonString(*image, "uri") : std::string{};
    if (uri.empty() || isDataUri(uri)) {
      return {};
    }
    const std::filesystem::path path = document.base_dir / uriToPath(uri);
    std::error_code error;
    return std::filesystem::exists(path, error) ? path
                                                : std::filesystem::path{};
  }

  /// Whether @p primitive is a triangle list, which is all this reads.
  bool isTriangles(const Json& primitive) {
    return jsonIndex(primitive, "mode").value_or(GLTF_TRIANGLES) ==
           GLTF_TRIANGLES;
  }

  /// Append every triangle primitive of mesh @p entry. The first failure,
  /// or none.
  std::optional<GltfLoadError> appendTriangles(const GltfDocument& document,
                                               const Json& entry,
                                               size_t skin_joints,
                                               SkinnedMeshData& mesh) {
    for (size_t p = 0; p < jsonArraySize(entry, "primitives"); ++p) {
      const Json& primitive = *jsonElement(entry, "primitives", p);
      if (!isTriangles(primitive)) {
        continue;
      }
      if (const auto error =
              appendPrimitive(document, primitive, skin_joints, mesh)) {
        return error;
      }
    }
    return std::nullopt;
  }

  /// The first triangle primitive of mesh @p entry, whose material the
  /// whole merged mesh is drawn with, or null.
  const Json* firstTriangles(const Json& entry) {
    for (size_t p = 0; p < jsonArraySize(entry, "primitives"); ++p) {
      const Json& primitive = *jsonElement(entry, "primitives", p);
      if (isTriangles(primitive)) {
        return &primitive;
      }
    }
    return nullptr;
  }

}  // namespace

std::expected<SkinnedMeshData, GltfLoadError>
buildGltfSkinnedMesh(const GltfDocument& document, size_t mesh,
                     size_t skin_joints) {
  const Json* entry = jsonElement(document.root, "meshes", mesh);
  if (entry == nullptr) {
    return std::unexpected(GltfLoadError::MALFORMED);
  }
  SkinnedMeshData out;
  if (const auto error = appendTriangles(document, *entry, skin_joints, out)) {
    return std::unexpected(*error);
  }
  if (out.indices.empty()) {
    return std::unexpected(GltfLoadError::NO_TRIANGLES);
  }
  setBounds(out);
  out.texture_path = texturePath(document, *firstTriangles(*entry));
  return out;
}

}  // namespace eng::gltf

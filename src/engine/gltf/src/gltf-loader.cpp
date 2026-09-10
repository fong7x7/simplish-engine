#include "glb-container.h"
#include "gltf-buffers.h"
#include "gltf-clip-builder.h"
#include "gltf-document.h"
#include "gltf-mesh-builder.h"
#include "gltf-rig-builder.h"

#include <engine/gltf/gltf-loader.h>
#include <utility>

namespace eng::gltf {

namespace {

  /// The one required extension this loader honours: it only widens the
  /// component types an accessor may use, which every read here converts.
  constexpr const char* QUANTIZATION = "KHR_mesh_quantization";

  /// Why the document's `asset` and `extensionsRequired` rule it out, if
  /// they do.
  std::optional<GltfLoadError> assetProblem(const Json& root) {
    const Json* asset = jsonMember(root, "asset");
    if (asset == nullptr || !jsonString(*asset, "version").starts_with('2')) {
      return GltfLoadError::UNSUPPORTED_VERSION;
    }
    const Json* required = jsonMember(root, "extensionsRequired");
    if (required == nullptr || !required->is_array()) {
      return std::nullopt;
    }
    for (const Json& name : *required) {
      if (!name.is_string() || name.get<std::string>() != QUANTIZATION) {
        return GltfLoadError::UNSUPPORTED_EXTENSION;
      }
    }
    return std::nullopt;
  }

  /// The mesh and skin of the first node that has both, or nothing.
  std::optional<std::pair<size_t, size_t>> skinnedMeshNode(const Json& root) {
    for (size_t n = 0; n < jsonArraySize(root, "nodes"); ++n) {
      const Json& node = *jsonElement(root, "nodes", n);
      const auto mesh = jsonIndex(node, "mesh");
      const auto skin = jsonIndex(node, "skin");
      if (mesh && skin) {
        return std::pair{*mesh, *skin};
      }
    }
    return std::nullopt;
  }

  /// The model around @p rig: its mesh and its clips.
  std::expected<SkinnedModel, GltfLoadError>
  modelFromRig(const GltfDocument& document, size_t mesh, GltfRig& rig) {
    auto triangles =
        buildGltfSkinnedMesh(document, mesh, rig.skin.joints.size());
    if (!triangles) {
      return std::unexpected(triangles.error());
    }
    auto clips = buildGltfClips(document, rig.joint_of_node);
    if (!clips) {
      return std::unexpected(clips.error());
    }
    return SkinnedModel{
        std::move(*triangles),
        {std::move(rig.skeleton), std::move(rig.skin), std::move(*clips)}};
  }

  /// The model in a document whose buffers are loaded.
  std::expected<SkinnedModel, GltfLoadError>
  assembleModel(const GltfDocument& document) {
    const auto node = skinnedMeshNode(document.root);
    if (!node) {
      return std::unexpected(GltfLoadError::NO_SKINNED_MESH);
    }
    auto rig = buildGltfRig(document, node->second);
    if (!rig) {
      return std::unexpected(rig.error());
    }
    return modelFromRig(document, node->first, *rig);
  }

}  // namespace

std::expected<SkinnedModel, GltfLoadError>
parseGltfModel(std::string_view json, std::span<const uint8_t> glb_bin,
               const std::filesystem::path& base_dir) {
  GltfDocument document;
  document.root = Json::parse(json.begin(), json.end(), nullptr, false);
  if (document.root.is_discarded() || !document.root.is_object()) {
    return std::unexpected(GltfLoadError::MALFORMED);
  }
  if (const auto problem = assetProblem(document.root)) {
    return std::unexpected(*problem);
  }
  document.base_dir = base_dir;
  if (const auto problem = loadGltfBuffers(document, glb_bin)) {
    return std::unexpected(*problem);
  }
  return assembleModel(document);
}

std::expected<SkinnedModel, GltfLoadError>
parseGlbModel(std::span<const uint8_t> glb,
              const std::filesystem::path& base_dir) {
  const auto chunks = splitGlb(glb);
  if (!chunks) {
    return std::unexpected(GltfLoadError::MALFORMED);
  }
  return parseGltfModel(chunks->json, chunks->bin, base_dir);
}

std::expected<SkinnedModel, GltfLoadError>
loadGltfModel(const std::filesystem::path& path) {
  const auto bytes = readFileBytes(path);
  if (!bytes) {
    return std::unexpected(GltfLoadError::UNREADABLE);
  }
  const auto base_dir = path.parent_path();
  if (looksLikeGlb(*bytes)) {
    return parseGlbModel(*bytes, base_dir);
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()),
                              bytes->size());
  return parseGltfModel(text, {}, base_dir);
}

}  // namespace eng::gltf

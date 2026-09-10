#pragma once

/// @file gltf-mesh-builder.h
/// @brief A glTF mesh's triangle primitives, merged into one skinned mesh.
/// @par Threading
/// Main thread only (checks the disk for the texture it names).

#include "gltf-document.h"

#include <engine/core/expected-polyfill.h>
#include <engine/gltf/gltf-load-error.h>
#include <engine/render-mesh/skinned-mesh-data.h>

namespace eng::gltf {

/// Every triangle primitive of mesh @p mesh, merged into one bind-pose
/// skinned mesh whose vertices name at most @p skin_joints palette entries.
///
/// Normals a primitive lacks are computed, smooth, from its triangles; a
/// vertex's weights are scaled to sum to one, since exporters round them;
/// and a vertex with no weights at all follows palette entry 0. Primitives
/// that are points or lines are skipped. The texture is the first
/// primitive's base colour map, when it is an image file that exists.
[[nodiscard]] std::expected<SkinnedMeshData, GltfLoadError>
buildGltfSkinnedMesh(const GltfDocument& document, size_t mesh,
                     size_t skin_joints);

}  // namespace eng::gltf

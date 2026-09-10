#pragma once

/// @file skinned-mesh-posing.h
/// @brief Skinning a mesh on the CPU into an ordinary static one.
/// @par Threading Thread-safe (pure function producing a new mesh).

#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-data.h>
#include <engine/render-mesh/skinned-mesh-data.h>
#include <span>

namespace eng {

/// @p mesh with every vertex moved by its joints' @p skin matrices — the
/// arithmetic the skinned vertex shaders do, done here once.
///
/// For what has no GPU: an asset thumbnail, which the editor rasterises on
/// the CPU, and tests, which can measure where a posed vertex landed. The
/// result is a plain `MeshData` whose bounds are the posed mesh's. An
/// empty @p skin gives the bind pose; a vertex naming a palette entry past
/// the end of @p skin is left where it was modelled for that influence.
[[nodiscard]] MeshData poseSkinnedMesh(const SkinnedMeshData& mesh,
                                       std::span<const Mat4> skin);

}  // namespace eng

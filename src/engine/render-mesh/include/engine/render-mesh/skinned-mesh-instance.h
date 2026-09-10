#pragma once

/// @file skinned-mesh-instance.h
/// @brief One placement of an uploaded skinned mesh, posed.

#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-instance.h>
#include <engine/render/rhi-core-types.h>
#include <span>

namespace eng {

/// A skinned mesh to draw, where to draw it, and the pose to draw it in.
///
/// The pose is borrowed, not held: `skin` points at matrices the caller
/// keeps alive until the draw has been recorded — a `RigPose`'s, typically,
/// evaluated this frame. Two instances of one mesh in two poses are two
/// instances with two spans.
/// @thread_safety Immutable value type; the span's target must outlive the
/// draw.
struct SkinnedMeshInstance {
  /// Which uploaded skinned mesh to draw.
  MeshGpuId mesh = MESH_GPU_INVALID;
  /// Object-to-world transform, applied after skinning.
  Mat4 model{};
  /// Diffuse map, or invalid for the untextured stand-in.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
  /// Skin matrix per palette entry, as `RigPose::evaluate` returns them.
  /// Empty draws the bind pose.
  std::span<const Mat4> skin{};
};

}  // namespace eng

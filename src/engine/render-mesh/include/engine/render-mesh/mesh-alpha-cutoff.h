#pragma once

/// @file mesh-alpha-cutoff.h
/// @brief The alpha a mesh's texel has to clear to be drawn at all.
/// @par Threading Thread-safe (immutable value type).

namespace eng {

/// Texels whose diffuse alpha is below this are discarded by the mesh
/// fragment stage rather than blended.
///
/// This is the alpha-test cutout
/// [ADR-003](../../../../../docs/decisions/ADR-003-hybrid-iso-render-model.md)
/// asks for: a sprite billboard's empty corners must not draw, and the
/// mesh pass is opaque and depth-writing, so the only way for them not to
/// is for their fragments not to exist. Cutting out rather than blending is
/// what lets a billboard write depth like any other mesh — occluding what
/// is behind it, occluded by what is in front — with no sorted pass and no
/// ordering rule.
///
/// A half is the conventional threshold, and it is the one value at which a
/// pair of frames drawn over each other with complementary alpha covers
/// every pixel exactly once. Opaque geometry is untouched: an image with no
/// alpha channel loads with every texel at 1, as does the untextured
/// stand-in.
///
/// Every backend's mesh fragment shader spells this number out again —
/// `MESH_MSL_SOURCE`, `MESH_HLSL_SOURCE`, and the two GLSL copies — since
/// none of them can include a C++ header. Changing it here means changing
/// it in all four.
inline constexpr float MESH_ALPHA_CUTOFF = 0.5f;

}  // namespace eng

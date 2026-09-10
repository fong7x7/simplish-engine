#pragma once

/// @file skin-palette.h
/// @brief A skin's matrices in the layout the skinned vertex stage reads.
/// @par Threading Thread-safe (immutable value type and a pure function).

#include <cstddef>
#include <engine/math/mat4.h>
#include <span>

namespace eng {

/// Most joints one skinned draw can move its mesh with.
///
/// Set by the smallest per-draw constant space across the backends: Metal
/// takes at most 4 KB of inline bytes per slot. A joint is three rows of
/// four floats — 48 bytes — so 85 would fit and 80 does with room to spare.
/// A full humanoid rig with finger bones is about 65.
///
/// Every skinned vertex shader restates this number to size its array —
/// `SKINNED_MSL` in `metal-device-impl.mm`, `MESH_HLSL_SOURCE` in
/// `dx12-builtin-pipelines.cpp`, and `SKINNED_VERTEX_SHADER_GLSL` in
/// `opengl-device.cpp` — so it moves in all four places or none.
inline constexpr size_t MESH_MAX_SKIN_JOINTS = 80;

/// Rows each joint occupies in the palette.
inline constexpr size_t SKIN_PALETTE_ROWS_PER_JOINT = 3;

/// Every joint's skin matrix, as the top three rows of each.
///
/// The bottom row of a skin matrix is always (0, 0, 0, 1) — a joint moves
/// a vertex, it never projects it — so it is dropped, which is a quarter of
/// the bytes every skinned draw sends. Rows rather than the columns `Mat4`
/// stores, because a vertex stage finds a skinned position as three dot
/// products of a row with the vertex, and a weighted sum of rows is the
/// weighted sum of the matrices. Sent whole with every draw, so the shader
/// never reads bytes the draw did not supply.
/// @thread_safety Immutable value type.
struct SkinPalette {
  /// Joint `k`'s rows are entries `3k`, `3k + 1`, and `3k + 2`, each four
  /// floats. Joints past the skin's length are the identity.
  float rows[MESH_MAX_SKIN_JOINTS * SKIN_PALETTE_ROWS_PER_JOINT][4]{};
};

static_assert(sizeof(SkinPalette) <= 4096,
              "the palette must fit Metal's inline vertex bytes");

/// The palette for @p skin: each matrix's top three rows, in order, and the
/// identity for every slot past the end. Matrices past
/// `MESH_MAX_SKIN_JOINTS` are dropped; a loader rejects a rig that has any.
[[nodiscard]] SkinPalette makeSkinPalette(std::span<const Mat4> skin);

}  // namespace eng

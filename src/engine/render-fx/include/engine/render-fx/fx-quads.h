#pragma once

/// @file fx-quads.h
/// @brief Laying live particles out as camera-facing quads.
/// @par Threading
/// Pure functions over their arguments.

#include <cstdint>
#include <engine/render-fx/fx-particle-pool.h>
#include <engine/render-fx/fx-quad-view.h>
#include <engine/render-fx/fx-vertex.h>
#include <utility>
#include <vector>

namespace eng {

/// Vertices one particle's quad takes: two triangles, unindexed.
inline constexpr uint32_t FX_VERTICES_PER_PARTICLE = 6;

/// Replace @p out with a quad for every live particle of @p particles,
/// farthest first, so smoke hides what is behind it and not what is in
/// front of it. @p order is scratch, kept by the caller so a frame does
/// not allocate once it has grown.
///
/// Each quad faces the camera at the one depth of the particle's centre —
/// never leaning into the floor — and is round in pixels; one whose look
/// streaks is stretched along the way it is moving on screen.
void buildFxQuads(const FxParticlePool& particles, const FxQuadView& view,
                  std::vector<std::pair<float, uint32_t>>& order,
                  std::vector<FxVertex>& out);

}  // namespace eng

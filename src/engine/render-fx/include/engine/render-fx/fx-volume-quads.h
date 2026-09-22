#pragma once

/// @file fx-volume-quads.h
/// @brief Lays every cloud of smoke out as the screen rectangle it covers.
/// @par Threading
/// Main-thread-only; touches nothing but its arguments.

#include <cstdint>
#include <engine/render-fx/fx-quad-view.h>
#include <engine/render-fx/fx-volume-pool.h>
#include <engine/render-fx/fx-volume-vertex.h>
#include <utility>
#include <vector>

namespace eng {

/// Corners one cloud is drawn with: two triangles over the rectangle its
/// box covers on screen.
inline constexpr uint32_t FX_VERTICES_PER_VOLUME = 6;

/// Write the rectangle of every live cloud in @p volumes into @p out,
/// farthest first so nearer smoke blends over it. @p order is scratch the
/// caller keeps so a frame allocates nothing.
///
/// A cloud whose box falls outside the frame, or whose camera has no
/// depth to march along, is left out. `out` is cleared first.
void buildFxVolumeQuads(const FxVolumePool& volumes, const FxQuadView& view,
                        std::vector<std::pair<float, uint32_t>>& order,
                        std::vector<FxVolumeVertex>& out);

}  // namespace eng

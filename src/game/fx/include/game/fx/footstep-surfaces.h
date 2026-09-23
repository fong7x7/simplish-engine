#pragma once

/// @file footstep-surfaces.h
/// @brief What every place in a level sounds like to walk on.
/// @par Threading
/// A value type; lookups are pure.

#include <engine/math/vec3.h>
#include <engine/render-ground/ground-grid.h>
#include <game/content/footstep-surface.h>
#include <game/fx/footstep-patch.h>
#include <vector>

namespace eng::game {

/// The surfaces a level's floor is made of: the painted ground, cell by
/// cell, and the props laid over it that sound like something else.
///
/// Built by whoever holds the level — the editor, from its document — and
/// read by `FootstepTracker`. Presentation: the simulation never reads it.
struct FootstepSurfaces {
  /// Each cell's surface, as a `FootstepSurface` number; bare cells, and
  /// every cell outside the grid, are `GROUND`.
  GroundGrid ground{};
  /// Props that override the ground under them, in level order: where two
  /// overlap, the later wins, as it is drawn over the earlier.
  std::vector<FootstepPatch> patches{};
};

/// How far above or below a patch's box, in tiles, feet still count as on
/// it — so a rug a thirty-second thick is underfoot for a character whose
/// feet are on the floor.
inline constexpr float FOOTSTEP_PATCH_REACH = 0.25F;

/// The surface under feet standing at @p feet: the last patch whose
/// footprint holds them, or else the ground's cell there.
[[nodiscard]] FootstepSurface footstepSurfaceAt(const FootstepSurfaces& level,
                                                Vec3 feet);

}  // namespace eng::game

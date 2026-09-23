#include <cmath>
#include <game/fx/footstep-surfaces.h>
#include <ranges>

namespace eng::game {

namespace {

  /// Whether feet at @p feet stand on @p patch: inside its footprint, and
  /// within reach of its height.
  bool standsOn(const FootstepPatch& patch, Vec3 feet) {
    return feet.x >= patch.min.x && feet.x <= patch.max.x &&
           feet.y >= patch.min.y && feet.y <= patch.max.y &&
           feet.z >= patch.min.z - FOOTSTEP_PATCH_REACH &&
           feet.z <= patch.max.z + FOOTSTEP_PATCH_REACH;
  }

  /// The ground's surface in the cell under @p feet.
  FootstepSurface groundAt(const GroundGrid& ground, Vec3 feet) {
    const auto limit = static_cast<float>(GROUND_COORDINATE_LIMIT);
    if (std::abs(feet.x) >= limit || std::abs(feet.y) >= limit) {
      return FootstepSurface::GROUND;
    }
    const uint8_t value = ground.at({static_cast<int32_t>(std::floor(feet.x)),
                                     static_cast<int32_t>(std::floor(feet.y))});
    return value < FOOTSTEP_SURFACE_COUNT ? ALL_FOOTSTEP_SURFACES[value]
                                          : FootstepSurface::GROUND;
  }

}  // namespace

FootstepSurface footstepSurfaceAt(const FootstepSurfaces& level, Vec3 feet) {
  // The latest first: where two overlap, the one laid last is on top.
  for (const FootstepPatch& patch : std::views::reverse(level.patches)) {
    if (standsOn(patch, feet)) {
      return patch.surface;
    }
  }
  return groundAt(level.ground, feet);
}

}  // namespace eng::game

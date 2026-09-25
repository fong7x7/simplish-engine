#include <cmath>
#include <game/world/walker-gait.h>

namespace eng::game {

bool walkOn(WalkerGait& gait, Vec3 at) {
  // sqrt, not hypot: sqrt is exact under IEEE on every machine, as the
  // tick needs (ADR-002); hypot is not promised to be.
  const float dx = at.x - gait.last.x;
  const float dy = at.y - gait.last.y;
  const float moved = std::sqrt(dx * dx + dy * dy);
  gait.last = at;
  if (gait.known == 0 || moved > WALKER_TELEPORT_TILES) {
    gait.known = 1;
    gait.travelled = gait.stride * 0.5F;
    return false;
  }
  gait.travelled += moved;
  if (gait.travelled < gait.stride) {
    return false;
  }
  gait.travelled = std::fmod(gait.travelled, gait.stride);
  return true;
}

}  // namespace eng::game

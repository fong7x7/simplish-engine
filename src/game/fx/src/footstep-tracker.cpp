#include <algorithm>
#include <cmath>
#include <game/fx/footstep-sounds.h>
#include <game/fx/footstep-tracker.h>

namespace eng::game {

namespace {

  /// How far into a stride a walker starts: half, for one just seen.
  constexpr float FIRST_STRIDE = 0.5F;
  /// How far into a stride a walker that stopped starts again, so its
  /// first step lands soon after it moves off.
  constexpr float RESUMED_STRIDE = 0.7F;
  /// Less than this, in tiles, is standing still.
  constexpr float STILL_TILES = 1e-4F;

  /// How far apart @p a and @p b are across the ground.
  float groundDistance(Vec3 a, Vec3 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
  }

  /// Move @p gait on to where @p walker now is, and say whether that
  /// completed a stride — carrying the rest on towards the next, one step a
  /// tick at most.
  bool strideDone(FootstepGait& gait, const FootstepWalker& walker) {
    const float stride = stepSetStride(walker.steps);
    const float moved = groundDistance(walker.at, gait.last);
    gait.last = walker.at;
    if (moved > FOOTSTEP_TELEPORT_TILES) {
      gait.travelled = stride * FIRST_STRIDE;
      return false;
    }
    if (moved < STILL_TILES) {
      gait.travelled = std::max(gait.travelled, stride * RESUMED_STRIDE);
      return false;
    }
    gait.travelled += moved;
    const bool done = gait.travelled >= stride;
    gait.travelled = std::fmod(gait.travelled, stride);
    return done;
  }

}  // namespace

void FootstepTracker::advance(std::span<const FootstepWalker> walkers,
                              const FootstepSurfaces& level,
                              std::vector<FootstepCue>& cues) {
  ++round_;
  for (const FootstepWalker& walker : walkers) {
    const float stride = stepSetStride(walker.steps);
    const auto [found, fresh] = gaits_.try_emplace(
        walker.key, FootstepGait{walker.at, stride * FIRST_STRIDE, round_});
    found->second.seen = round_;
    if (!fresh && strideDone(found->second, walker)) {
      cues.push_back(
          {walker.at, walker.steps, footstepSurfaceAt(level, walker.at)});
    }
  }
  std::erase_if(gaits_, [this](const auto& entry) {
    return entry.second.seen != round_;
  });
}

}  // namespace eng::game

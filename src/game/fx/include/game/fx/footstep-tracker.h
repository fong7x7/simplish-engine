#pragma once

/// @file footstep-tracker.h
/// @brief When somebody walking takes a step.
/// @par Threading
/// Main-thread-only. Presentation, run after the tick it reads.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/fx/footstep-cue.h>
#include <game/fx/footstep-gait.h>
#include <game/fx/footstep-surfaces.h>
#include <game/fx/footstep-walker.h>
#include <map>
#include <span>
#include <vector>

namespace eng::game {

/// How far, in tiles, somebody may move in one tick and still be walking.
/// Further is a spawn or a jump across the level, which is no step.
inline constexpr float FOOTSTEP_TELEPORT_TILES = 1.0F;

/// Counts the ground each walker covers and cues a step every stride.
///
/// Timed by distance rather than by animation, so a sprite, a static model
/// and a rigged one all step alike, and a faster walker steps more often
/// without being told to. Reads positions the simulation left and writes
/// nothing back: steps are presentation, and only the sound depends on
/// them.
class FootstepTracker {
public:
  /// Advance every one of @p walkers to where it now is, and append a cue
  /// to @p cues for each that completed a stride, on the surface of
  /// @p level under it. A walker not seen before starts half a stride in;
  /// one that stopped starts again most of a stride in, so its first step
  /// comes promptly; one missing from @p walkers is forgotten.
  void advance(std::span<const FootstepWalker> walkers,
               const FootstepSurfaces& level, std::vector<FootstepCue>& cues);

  /// How many walkers are being followed.
  [[nodiscard]] size_t walkers() const { return gaits_.size(); }

private:
  /// Every walker being followed, by key. Sorted, so forgetting the ones
  /// that went goes in one order every time.
  std::map<uint32_t, FootstepGait> gaits_{};
  /// How many times `advance` has run.
  uint64_t round_ = 0;
};

}  // namespace eng::game

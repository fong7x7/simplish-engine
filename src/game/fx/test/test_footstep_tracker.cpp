#include <catch2/catch_test_macros.hpp>
#include <game/fx/footstep-sounds.h>
#include <game/fx/footstep-tracker.h>
#include <vector>

using namespace eng;
using namespace eng::game;

namespace {

/// Walk walker @p key along X from @p from by @p per_tick tiles for
/// @p ticks ticks, and give back every step it took.
std::vector<FootstepCue> walk(FootstepTracker& tracker, StepSet steps,
                              float per_tick, int ticks) {
  const FootstepSurfaces level;
  std::vector<FootstepCue> cues;
  for (int tick = 0; tick <= ticks; ++tick) {
    const FootstepWalker walker{
        7, {per_tick * static_cast<float>(tick), 0, 0}, steps};
    tracker.advance({&walker, 1}, level, cues);
  }
  return cues;
}

}  // namespace

TEST_CASE("a walker steps once a stride") {
  FootstepTracker tracker;
  const float stride = stepSetStride(StepSet::DEFAULT);
  // Ten strides at a tenth of a tile a tick: the first comes half a stride
  // in, then one every stride.
  const auto cues = walk(tracker, StepSet::DEFAULT, 0.1F,
                         static_cast<int>(10.0F * stride / 0.1F));
  REQUIRE(cues.size() >= 9);
  REQUIRE(cues.size() <= 11);
}

TEST_CASE("claws patter faster than heavy feet stomp") {
  FootstepTracker claws;
  FootstepTracker heavy;
  const auto quick = walk(claws, StepSet::CLAWS, 0.1F, 100);
  const auto slow = walk(heavy, StepSet::HEAVY, 0.1F, 100);
  REQUIRE(quick.size() > slow.size() * 2);
}

TEST_CASE("standing still takes no steps") {
  FootstepTracker tracker;
  REQUIRE(walk(tracker, StepSet::BOOTS, 0.0F, 200).empty());
}

TEST_CASE("a jump across the level is not a step") {
  FootstepTracker tracker;
  const FootstepSurfaces level;
  std::vector<FootstepCue> cues;
  const FootstepWalker here{1, {0, 0, 0}, StepSet::DEFAULT};
  const FootstepWalker there{1, {40, 0, 0}, StepSet::DEFAULT};
  tracker.advance({&here, 1}, level, cues);
  tracker.advance({&there, 1}, level, cues);
  REQUIRE(cues.empty());
}

TEST_CASE("a walker that is gone is forgotten") {
  FootstepTracker tracker;
  const FootstepSurfaces level;
  std::vector<FootstepCue> cues;
  const FootstepWalker walker{3, {0, 0, 0}, StepSet::DEFAULT};
  tracker.advance({&walker, 1}, level, cues);
  REQUIRE(tracker.walkers() == 1);
  tracker.advance({}, level, cues);
  REQUIRE(tracker.walkers() == 0);
}

TEST_CASE("each step lands on the surface under it") {
  FootstepTracker tracker;
  FootstepSurfaces level;
  level.ground.set({0, 0}, static_cast<uint8_t>(FootstepSurface::SAND));
  std::vector<FootstepCue> cues;
  for (int tick = 0; tick < 20; ++tick) {
    const FootstepWalker walker{
        1, {0.05F * static_cast<float>(tick), 0.5F, 0}, StepSet::BARE};
    tracker.advance({&walker, 1}, level, cues);
  }
  REQUIRE_FALSE(cues.empty());
  REQUIRE(cues.front().surface == FootstepSurface::SAND);
  REQUIRE(cues.front().steps == StepSet::BARE);
}

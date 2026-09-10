#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/core/fixed-step-clock.h>

using eng::FixedStepAdvance;
using eng::FixedStepClock;

namespace {

/// One 60 Hz frame, rounded up to whole nanoseconds as a real clock reports.
constexpr uint64_t FRAME_NS = 16'666'667;

}  // namespace

TEST_CASE("FixedStepClock runs nothing until a whole tick has elapsed") {
  FixedStepClock clock;
  const FixedStepAdvance advance = clock.advance(10'000'000);
  CHECK(advance.ticks == 0);
  CHECK(advance.dropped_ticks == 0);
  CHECK(advance.interpolation == Catch::Approx(0.6).epsilon(1e-6));
}

TEST_CASE("FixedStepClock turns sixty frames into sixty ticks") {
  FixedStepClock clock;
  uint32_t ticks = 0;
  for (int i = 0; i < 60; ++i) {
    ticks += clock.advance(FRAME_NS).ticks;
  }
  CHECK(ticks == 60);
}

TEST_CASE("FixedStepClock does not drift over an hour of 60 Hz frames") {
  // 1/60 s is not a whole number of nanoseconds; an accumulator in plain
  // nanoseconds would lose or gain a tick every few seconds.
  FixedStepClock clock;
  uint64_t ticks = 0;
  for (int second = 0; second < 3600; ++second) {
    for (int frame = 0; frame < 60; ++frame) {
      const uint64_t ns = frame % 3 == 2 ? 16'666'666 : 16'666'667;
      ticks += clock.advance(ns).ticks;
    }
  }
  CHECK(ticks == 3600U * 60U);
}

TEST_CASE("FixedStepClock clamps a long frame and reports the shortfall") {
  FixedStepClock clock;
  const FixedStepAdvance advance = clock.advance(FRAME_NS * 10);
  CHECK(advance.ticks == eng::MAX_TICKS_PER_FRAME);
  CHECK(advance.dropped_ticks == 6);
}

TEST_CASE("FixedStepClock does not carry dropped ticks into the next frame") {
  FixedStepClock clock;
  (void)clock.advance(FRAME_NS * 10);
  CHECK(clock.advance(FRAME_NS).ticks == 1);
}

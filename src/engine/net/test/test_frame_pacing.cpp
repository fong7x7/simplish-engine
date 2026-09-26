#include <catch2/catch_test_macros.hpp>
#include <engine/net/frame-pacing.h>

using eng::net::framesToStep;

TEST_CASE("a client steps what its clock owes, when that has arrived") {
  CHECK(framesToStep(1, 1) == 1);
  CHECK(framesToStep(2, 2) == 2);
  CHECK(framesToStep(1, 0) == 0);  // Stalled: nothing to step.
  CHECK(framesToStep(3, 1) == 1);  // Owed more than came: all that came.
}

TEST_CASE("a client behind eats into its backlog a little each frame") {
  CHECK(framesToStep(1, 2) == 2);   // One behind: caught up at once.
  CHECK(framesToStep(1, 9) == 2);   // Eight behind: one extra.
  CHECK(framesToStep(1, 61) == 9);  // A second behind: eight extra.
  CHECK(framesToStep(0, 5) == 1);   // Owed nothing, still catches up.
}

TEST_CASE("a second's backlog is gone in about a fifth of a second") {
  std::size_t waiting = 61;
  int frames = 0;
  while (waiting > 1) {
    waiting -= framesToStep(1, waiting);
    ++waiting;  // Another frame arrives every render frame.
    ++frames;
  }
  CHECK(frames > 1);
  CHECK(frames <= 20);
}

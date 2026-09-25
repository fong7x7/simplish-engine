#include <catch2/catch_test_macros.hpp>
#include <game/world/walker-gait.h>

using eng::game::WalkerGait;
using eng::game::walkOn;

TEST_CASE("a walker steps each stride, starting half a stride in") {
  WalkerGait gait{.stride = 0.75F};
  float x = 0.0F;

  CHECK_FALSE(walkOn(gait, {x, 0.0F, 0.0F}));  // first seen: 0.375 in
  CHECK_FALSE(walkOn(gait, {x += 0.2F, 0.0F, 0.0F}));
  CHECK(walkOn(gait, {x += 0.2F, 0.0F, 0.0F}));  // 0.775: a step
  CHECK_FALSE(walkOn(gait, {x += 0.2F, 0.0F, 0.0F}));
  CHECK_FALSE(walkOn(gait, {x, 0.0F, 0.0F}));  // standing still
}

TEST_CASE("a walker moved further than a walk in a tick takes no step") {
  WalkerGait gait{.stride = 0.75F};
  (void)walkOn(gait, {0.0F, 0.0F, 0.0F});

  CHECK_FALSE(walkOn(gait, {5.0F, 0.0F, 0.0F}));
  CHECK(gait.travelled == 0.375F);
}

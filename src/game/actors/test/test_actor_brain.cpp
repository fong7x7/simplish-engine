#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <game/actors/actor-brain.h>

using Catch::Approx;
using eng::game::BehaviorDefinition;
using eng::game::compileBrain;

TEST_CASE("a brain works its behavior's rates out per tick") {
  BehaviorDefinition behavior;
  behavior.movement.speed = 6.0F;
  behavior.movement.turn_degrees_per_second = 360.0F;
  behavior.senses.sight_range = 10.0F;
  behavior.senses.hearing_range = 3.0F;
  const auto brain = compileBrain(behavior);

  REQUIRE(brain.speed_per_tick == Approx(0.1F));
  // Six degrees a tick.
  REQUIRE(brain.turn_per_tick.cos == Approx(0.9945219F));
  REQUIRE(brain.sight_squared == 100.0F);
  REQUIRE(brain.hearing_squared == 9.0F);
}

TEST_CASE("a brain's view cone is half its view either side of its facing") {
  BehaviorDefinition behavior;
  behavior.senses.view_degrees = 180.0F;
  REQUIRE(compileBrain(behavior).view_cos == 0.0F);
  behavior.senses.view_degrees = 120.0F;
  REQUIRE(compileBrain(behavior).view_cos == Approx(0.5F));
  behavior.senses.view_degrees = 360.0F;
  REQUIRE(compileBrain(behavior).view_cos < -1.0F);
}

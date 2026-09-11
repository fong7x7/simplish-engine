#include "support/actor-arena.h"

#include <catch2/catch_test_macros.hpp>
#include <game/content/behavior-lookup.h>
#include <vector>

using eng::Vec2;
using eng::game::BehaviorAction;
using eng::game::BehaviorCondition;
using eng::game::BehaviorDefinition;
using eng::game::BehaviorRouteMode;
using eng::game::defaultBehaviorState;
using eng::game::test::ActorArena;

namespace {

/// A behavior that only patrols, as @p mode says.
BehaviorDefinition patroller(BehaviorRouteMode mode) {
  BehaviorDefinition behavior;
  behavior.id = "patroller";
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::PATROL));
  behavior.states.back().id = "patrol";
  behavior.states.back().route = mode;
  return behavior;
}

/// Three waypoints along a line, three tiles apart.
const std::vector<Vec2> LINE{{0.0F, 0.0F}, {3.0F, 0.0F}, {6.0F, 0.0F}};

/// Every leg actor @p actor walks to over @p ticks ticks, in order, each
/// once until it changes.
std::vector<uint16_t> legsWalked(ActorArena& arena, uint32_t actor, int ticks) {
  std::vector<uint16_t> legs{arena.actors.route_leg[actor]};
  for (int tick = 0; tick < ticks; ++tick) {
    arena.step();
    if (arena.actors.route_leg[actor] != legs.back()) {
      legs.push_back(arena.actors.route_leg[actor]);
    }
  }
  return legs;
}

}  // namespace

TEST_CASE("a looping patrol walks its route round and round") {
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(patroller(BehaviorRouteMode::LOOP),
                     {.at = {0.0F, 0.0F, 0.0F}, .route = LINE});
  const auto legs = legsWalked(arena, actor, 900);
  REQUIRE(legs.size() >= 5);
  REQUIRE(std::vector<uint16_t>(legs.begin(), legs.begin() + 5) ==
          std::vector<uint16_t>{0, 1, 2, 0, 1});
}

TEST_CASE("a ping-pong patrol walks its route back and forth") {
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(patroller(BehaviorRouteMode::PING_PONG),
                     {.at = {0.0F, 0.0F, 0.0F}, .route = LINE});
  const auto legs = legsWalked(arena, actor, 900);
  REQUIRE(legs.size() >= 5);
  REQUIRE(std::vector<uint16_t>(legs.begin(), legs.begin() + 5) ==
          std::vector<uint16_t>{0, 1, 2, 1, 0});
}

TEST_CASE("a patrol reaches each waypoint it walks to") {
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(patroller(BehaviorRouteMode::LOOP),
                     {.at = {0.0F, 0.0F, 0.0F}, .route = LINE});
  float nearest = 99.0F;
  for (int tick = 0; tick < 200; ++tick) {
    arena.step();
    nearest = std::min(nearest, Vec2::distance(arena.actorAt(actor), LINE[2]));
  }
  REQUIRE(nearest <= 0.25F);
}

TEST_CASE("an actor with no route stands where it is") {
  ActorArena arena;
  const uint32_t actor = arena.addActor(patroller(BehaviorRouteMode::LOOP),
                                        {.at = {1.0F, 2.0F, 0.0F}});
  arena.step(120);
  REQUIRE(arena.actorAt(actor).x == 1.0F);
  REQUIRE(arena.actors.arrived[actor] == 1);
}

TEST_CASE("a patrol picks its round up where it left it") {
  BehaviorDefinition behavior = patroller(BehaviorRouteMode::LOOP);
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
  behavior.states.back().id = "pause";
  behavior.states.back().exits.push_back(
      {.when = BehaviorCondition::IN_STATE_FOR, .ticks = 30, .to = 0});
  behavior.states[0].exits.push_back(
      {.when = BehaviorCondition::ARRIVED, .to = 1});
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(behavior, {.at = {0.0F, 0.0F, 0.0F}, .route = LINE});
  const auto legs = legsWalked(arena, actor, 900);
  // Each arrival pauses it, and each pause resumes the round at the next
  // waypoint rather than back at the first.
  REQUIRE(legs.size() >= 4);
  REQUIRE(std::vector<uint16_t>(legs.begin(), legs.begin() + 4) ==
          std::vector<uint16_t>{0, 1, 2, 0});
}

TEST_CASE("the patrol preset strolls its round and gives chase on sight") {
  ActorArena arena;
  const uint32_t actor = arena.addActor(
      eng::game::resolveBehavior({}, "patrol"),
      {.at = {0.0F, 0.0F, 0.0F}, .yaw_degrees = 0.0F, .route = LINE});
  arena.step(30);
  REQUIRE(arena.stateOf(actor) == "patrol");
  arena.addPlayer({4.0F, 1.0F});
  arena.step();
  REQUIRE(arena.stateOf(actor) == "pursue");
}

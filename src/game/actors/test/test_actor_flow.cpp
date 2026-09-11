// Pursuit by flow field: the players' fields, and actors walking them.

#include "support/actor-arena.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-action.h>
#include <game/content/behavior-lookup.h>

using eng::Vec2;
using eng::game::ACTOR_FLOW_IDLE;
using eng::game::resolveBehavior;
using eng::game::test::ActorArena;

namespace {

/// A wall from y = -10 to 10 at x = 0, between the actors' side and the
/// players'.
const std::vector<eng::physics::CollisionBox> WALL{
    {{0.0F, -10.0F, 0.0F}, {0.5F, 10.0F, 2.0F}}};

/// The chase preset, hearing a shot across the arena.
eng::game::BehaviorDefinition keenChase() {
  eng::game::BehaviorDefinition behavior = resolveBehavior({}, "chase");
  behavior.senses.hearing_range = 30.0F;
  return behavior;
}

/// Step @p arena until its flow fields are idle, at most @p limit ticks.
void settle(ActorArena& arena, uint32_t limit) {
  for (uint32_t t = 0; t < limit && arena.flow.building != ACTOR_FLOW_IDLE;
       ++t) {
    arena.step();
  }
}

}  // namespace

TEST_CASE("a player's field is built toward the cell they stand in") {
  ActorArena arena(WALL);
  arena.addPlayer({6.0F, 0.0F});
  arena.step();
  settle(arena, 200);

  const auto& field = arena.flow.fields[0];
  REQUIRE(field.complete());
  REQUIRE(field.goal() == *arena.grid.cellAt({6.0F, 0.0F}));
  // Nothing moved, so nothing is rebuilt.
  arena.step(5);
  REQUIRE(arena.flow.building == ACTOR_FLOW_IDLE);
}

TEST_CASE("a player who moves has their field rebuilt toward where they went") {
  ActorArena arena(WALL);
  arena.addPlayer({6.0F, 0.0F});
  arena.step();
  settle(arena, 200);
  arena.movePlayer(0, {8.0F, 3.0F});
  arena.step();
  settle(arena, 200);
  REQUIRE(arena.flow.fields[0].goal() == *arena.grid.cellAt({8.0F, 3.0F}));
}

TEST_CASE("a pursuer that hears a player beyond a wall goes round it down "
          "their field") {
  ActorArena arena(WALL);
  arena.addPlayer({8.0F, 0.0F});
  arena.setFiring(0, eng::input::INPUT_BUTTON_FIRE);
  const uint32_t actor =
      arena.addActor(keenChase(), {.at = {-8.0F, 0.0F, 0.0F}});
  arena.step();
  settle(arena, 200);
  arena.step(20);
  // Far off, it keeps to the field: one waypoint a tile down it.
  REQUIRE(arena.actors.path[actor].count == 1);
  REQUIRE(arena.actors.path[actor].goal == arena.flow.fields[0].goal());

  arena.step(600);
  REQUIRE(Vec2::distance(arena.actorAt(actor), {8.0F, 0.0F}) < 1.5F);
}

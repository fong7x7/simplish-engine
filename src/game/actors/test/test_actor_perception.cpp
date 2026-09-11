#include "support/actor-arena.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/input-action.h>
#include <game/content/behavior-lookup.h>

using eng::game::BehaviorAction;
using eng::game::BehaviorDefinition;
using eng::game::defaultBehaviorState;
using eng::game::Faction;
using eng::game::test::ActorArena;

namespace {

/// A behavior that stands still and watches, seeing 8 tiles over a 180°
/// view, hearing 4, and remembering for 60 ticks.
BehaviorDefinition watcher() {
  BehaviorDefinition behavior;
  behavior.id = "watcher";
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
  behavior.states.back().facing = eng::game::BehaviorFacing::LOCKED;
  behavior.senses.sight_range = 8.0F;
  behavior.senses.view_degrees = 180.0F;
  behavior.senses.hearing_range = 4.0F;
  behavior.senses.memory_ticks = 60;
  return behavior;
}

/// A wall across x = 2 to 2.5, from y = -4 to 4.
const std::vector<eng::physics::CollisionBox> WALL{
    {{2.0F, -4.0F, 0.0F}, {2.5F, 4.0F, 2.0F}}};

}  // namespace

TEST_CASE("an actor sees a player in range, in front, in the open") {
  ActorArena arena;
  const uint32_t player = arena.addPlayer({5.0F, 1.0F});
  const uint32_t actor = arena.addActor(watcher(), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();

  REQUIRE(arena.actors.sees_target[actor] == 1);
  REQUIRE(arena.actors.target[actor] == arena.players.slots.handleAt(player));
  REQUIRE(arena.actors.last_seen[actor].x == 5.0F);
}

TEST_CASE("an actor does not see behind it, or past its range") {
  ActorArena arena;
  arena.addPlayer({-3.0F, 0.0F});
  arena.addPlayer({9.0F, 0.0F});
  const uint32_t actor = arena.addActor(watcher(), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();
  REQUIRE(arena.actors.sees_target[actor] == 0);
  REQUIRE(arena.actors.remembers_target[actor] == 0);
}

TEST_CASE("walls block sight, but not sound") {
  ActorArena arena(WALL);
  const uint32_t player = arena.addPlayer({3.5F, 0.0F});
  const uint32_t actor = arena.addActor(watcher(), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();
  REQUIRE(arena.actors.sees_target[actor] == 0);

  arena.setFiring(player, eng::input::INPUT_BUTTON_FIRE);
  arena.step();
  REQUIRE(arena.actors.hears_target[actor] == 1);
  REQUIRE(arena.actors.sees_target[actor] == 0);
}

TEST_CASE("an actor remembers where it lost a target, then forgets") {
  ActorArena arena(WALL);
  const uint32_t player = arena.addPlayer({1.5F, 0.0F});
  const uint32_t actor = arena.addActor(watcher(), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();
  arena.movePlayer(player, {5.0F, 0.0F});
  arena.step(30);

  REQUIRE(arena.actors.sees_target[actor] == 0);
  REQUIRE(arena.actors.remembers_target[actor] == 1);
  REQUIRE(arena.actors.last_seen[actor].x == 1.5F);
  arena.step(40);
  REQUIRE(arena.actors.remembers_target[actor] == 0);
}

TEST_CASE("an actor keeps its target over a nearer newcomer") {
  ActorArena arena;
  const uint32_t first = arena.addPlayer({6.0F, 0.0F});
  const uint32_t actor = arena.addActor(watcher(), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();
  arena.addPlayer({2.0F, 0.0F});
  arena.step();
  REQUIRE(arena.actors.target[actor] == arena.players.slots.handleAt(first));
}

TEST_CASE("of two new players, an actor takes the nearer") {
  ActorArena arena;
  arena.addPlayer({6.0F, 0.0F});
  const uint32_t near = arena.addPlayer({2.0F, 1.0F});
  const uint32_t actor = arena.addActor(watcher(), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();
  REQUIRE(arena.actors.target[actor] == arena.players.slots.handleAt(near));
}

TEST_CASE("a neutral actor takes nobody as a target") {
  ActorArena arena;
  arena.addPlayer({2.0F, 0.0F});
  const uint32_t actor = arena.addActor(
      watcher(), {.at = {0.0F, 0.0F, 0.0F}, .faction = Faction::NEUTRAL});
  arena.step();
  REQUIRE(arena.actors.sees_target[actor] == 0);
  REQUIRE(arena.actors.remembers_target[actor] == 0);
}

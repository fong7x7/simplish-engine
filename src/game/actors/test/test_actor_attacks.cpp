// What attacking states put in the effects buffer, the conditions that read
// health, and an actor being hurt.

#include "support/actor-arena.h"

#include <catch2/catch_test_macros.hpp>
#include <game/actors/actor-system.h>
#include <game/content/behavior-lookup.h>
#include <game/player/player-system.h>

using namespace eng;
using namespace eng::game;
using eng::game::test::ActorArena;

namespace {

/// A behavior of one state doing @p action, seeing all round, 12 tiles.
BehaviorDefinition oneState(BehaviorAction action) {
  BehaviorDefinition behavior;
  behavior.id = "one";
  behavior.states.push_back(defaultBehaviorState(action));
  behavior.states.back().id = "only";
  behavior.senses.sight_range = 12.0F;
  behavior.senses.view_degrees = 360.0F;
  return behavior;
}

/// A behavior that goes from state `a` to `b` when @p exit holds.
BehaviorDefinition twoStates(BehaviorExit exit) {
  BehaviorDefinition behavior = oneState(BehaviorAction::HOLD);
  behavior.states[0].id = "a";
  behavior.states[0].exits.push_back(exit);
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
  behavior.states.back().id = "b";
  return behavior;
}

}  // namespace

TEST_CASE("a melee closes in and bites, once a cooldown") {
  ActorArena arena;
  const uint32_t player = arena.addPlayer({3.0F, 0.0F});
  arena.addActor(oneState(BehaviorAction::MELEE), {.at = {0.0F, 0.0F, 0.0F}});

  arena.step(60);
  REQUIRE_FALSE(arena.effects.damage.empty());
  const DamageEvent& bite = arena.effects.damage.front();
  REQUIRE(bite.target.kind == CombatantKind::PLAYER);
  REQUIRE(bite.target.handle == arena.players.slots.handleAt(player));
  REQUIRE(bite.amount == 1);
  const size_t bites = arena.effects.damage.size();
  arena.step(45);
  REQUIRE(arena.effects.damage.size() == bites + 1);
}

TEST_CASE("a volley fans its shots across the spread, at the target") {
  ActorArena arena;
  arena.addPlayer({6.0F, 0.0F});
  arena.addActor(oneState(BehaviorAction::FIRE), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();

  REQUIRE(arena.effects.shots.size() == 3);
  REQUIRE(arena.effects.shots[1].velocity.y == 0.0F);
  REQUIRE(arena.effects.shots[1].velocity.x > 0.0F);
  REQUIRE(arena.effects.shots[0].velocity.y < 0.0F);
  REQUIRE(arena.effects.shots[2].velocity.y > 0.0F);
  REQUIRE(arena.effects.shots[0].side == Faction::HOSTILE);
  arena.step(89);
  REQUIRE(arena.effects.shots.size() == 3);
  arena.step();
  REQUIRE(arena.effects.shots.size() == 6);
}

TEST_CASE("a spitter lobs a pool where it saw its target") {
  ActorArena arena;
  arena.addPlayer({5.0F, 2.0F});
  arena.addActor(oneState(BehaviorAction::SPIT), {.at = {0.0F, 0.0F, 0.0F}});
  arena.step();
  REQUIRE(arena.effects.hazards.size() == 1);
  REQUIRE(arena.effects.hazards[0].at.x == 5.0F);
  REQUIRE(arena.effects.hazards[0].ticks == 300);
}

TEST_CASE("an actor that detonates goes off in its blast, and dies") {
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(oneState(BehaviorAction::DETONATE), {.at = {0, 0, 0}});
  arena.step();
  REQUIRE(arena.actors.death_blast_radius[actor] == 2.0F);
  REQUIRE(arena.effects.damage.size() == 1);

  hurtActor(arena.actors, actor, {arena.effects.damage[0].amount, 1},
            arena.effects);
  REQUIRE(arena.actors.health[actor] == 0);
  REQUIRE(
      arena.actors.slots.isPendingDestroy(arena.actors.slots.handleAt(actor)));
  REQUIRE(arena.effects.blasts.size() == 1);
  REQUIRE(arena.effects.blasts[0].damage == 2);
}

TEST_CASE("an actor hurt remembers when, and one dead takes nothing more") {
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(oneState(BehaviorAction::HOLD), {.at = {0, 0, 0}});
  hurtActor(arena.actors, actor, {1, 7}, arena.effects);
  REQUIRE(arena.actors.health[actor] == ACTOR_DEFAULT_HEALTH - 1);
  REQUIRE(arena.actors.damaged_tick[actor] == 7);
  hurtActor(arena.actors, actor, {99, 8}, arena.effects);
  hurtActor(arena.actors, actor, {1, 9}, arena.effects);
  REQUIRE(arena.actors.damaged_tick[actor] == 8);
  // It had no blast of its own to go off in.
  REQUIRE(arena.effects.blasts.empty());
}

TEST_CASE("damaged and health_below read the actor's health") {
  ActorArena arena;
  const uint32_t hurt = arena.addActor(
      twoStates({.when = BehaviorCondition::DAMAGED, .ticks = 5, .to = 1}),
      {.at = {0, 0, 0}});
  const uint32_t weak = arena.addActor(
      twoStates(
          {.when = BehaviorCondition::HEALTH_BELOW, .permille = 500, .to = 1}),
      {.at = {5, 0, 0}});
  arena.step();
  REQUIRE(arena.stateOf(hurt) == "a");
  REQUIRE(arena.stateOf(weak) == "a");

  hurtActor(arena.actors, hurt, {1, arena.tick - 1}, arena.effects);
  hurtActor(arena.actors, weak, {2, arena.tick - 1}, arena.effects);
  arena.step();
  REQUIRE(arena.stateOf(hurt) == "b");
  REQUIRE(arena.stateOf(weak) == "b");
}

TEST_CASE("allies_within finds another actor on its side, and only that") {
  ActorArena arena;
  const BehaviorDefinition huddle = twoStates(
      {.when = BehaviorCondition::ALLIES_WITHIN, .tiles = 2.0F, .to = 1});
  const uint32_t alone = arena.addActor(huddle, {.at = {-8, 0, 0}});
  arena.addActor(huddle, {.at = {-6.5F, 0, 0}, .faction = Faction::FRIENDLY});
  const uint32_t pair = arena.addActor(huddle, {.at = {5, 0, 0}});
  arena.addActor(huddle, {.at = {6.5F, 0, 0}});
  arena.step();
  REQUIRE(arena.stateOf(alone) == "a");
  REQUIRE(arena.stateOf(pair) == "b");
}

TEST_CASE("a downed player is nobody's target") {
  ActorArena arena;
  const uint32_t player = arena.addPlayer({3.0F, 0.0F});
  const uint32_t actor =
      arena.addActor(oneState(BehaviorAction::MELEE), {.at = {0, 0, 0}});
  hurtPlayer(arena.players, player, 99, 0);
  arena.step(5);
  REQUIRE(arena.actors.remembers_target[actor] == 0);
  REQUIRE(arena.effects.damage.empty());
}

// Actors taking other actors as targets, and the near and far tiers of
// perception.

#include "support/actor-arena.h"

#include <catch2/catch_test_macros.hpp>
#include <game/actors/actor-system.h>
#include <game/content/behavior-lookup.h>

using eng::game::BehaviorAction;
using eng::game::BehaviorDefinition;
using eng::game::BehaviorTargets;
using eng::game::CombatantKind;
using eng::game::defaultBehaviorState;
using eng::game::Faction;
using eng::game::test::ActorArena;

namespace {

/// A behavior that stands and watches all round, 10 tiles, targeting
/// @p targets.
BehaviorDefinition sentry(BehaviorTargets targets) {
  BehaviorDefinition behavior;
  behavior.id = "sentry";
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
  behavior.senses.sight_range = 10.0F;
  behavior.senses.view_degrees = 360.0F;
  behavior.senses.targets = targets;
  return behavior;
}

/// A hostile actor that stands still and targets nobody: something to be
/// seen.
eng::game::ActorSpawn raiderAt(float x, float y) {
  return {.at = {x, y, 0.0F}, .faction = Faction::HOSTILE};
}

}  // namespace

TEST_CASE("a friendly actor targeting opponents takes a hostile one") {
  ActorArena arena;
  arena.addPlayer({-6.0F, 0.0F});
  const uint32_t guard =
      arena.addActor(sentry(BehaviorTargets::OPPONENTS),
                     {.at = {0.0F, 0.0F, 0.0F}, .faction = Faction::FRIENDLY});
  const uint32_t raider =
      arena.addActor(sentry(BehaviorTargets::PLAYERS), raiderAt(4.0F, 0.0F));
  arena.step();

  REQUIRE(arena.actors.target_kind[guard] == CombatantKind::ACTOR);
  REQUIRE(arena.actors.target[guard] == arena.actors.slots.handleAt(raider));
  REQUIRE(arena.actors.sees_target[guard] == 1);
  REQUIRE(arena.actors.last_seen[guard].x == 4.0F);
}

TEST_CASE("a friendly actor targeting players keeps to them, whoever is near") {
  ActorArena arena;
  const uint32_t player = arena.addPlayer({-6.0F, 0.0F});
  const uint32_t follower =
      arena.addActor(sentry(BehaviorTargets::PLAYERS),
                     {.at = {0.0F, 0.0F, 0.0F}, .faction = Faction::FRIENDLY});
  arena.addActor(sentry(BehaviorTargets::PLAYERS), raiderAt(2.0F, 0.0F));
  arena.step();

  REQUIRE(arena.actors.target_kind[follower] == CombatantKind::PLAYER);
  REQUIRE(arena.actors.target[follower] ==
          arena.players.slots.handleAt(player));
}

TEST_CASE("a hostile actor targeting opponents takes the nearer of a player "
          "and a friendly actor") {
  ActorArena arena;
  arena.addPlayer({7.0F, 0.0F});
  const uint32_t raider =
      arena.addActor(sentry(BehaviorTargets::OPPONENTS), raiderAt(0.0F, 0.0F));
  const uint32_t ally =
      arena.addActor(sentry(BehaviorTargets::PLAYERS),
                     {.at = {-3.0F, 0.0F, 0.0F}, .faction = Faction::FRIENDLY});
  arena.addActor(sentry(BehaviorTargets::PLAYERS),
                 {.at = {1.0F, 0.0F, 0.0F}, .faction = Faction::NEUTRAL});
  arena.step();

  REQUIRE(arena.actors.target_kind[raider] == CombatantKind::ACTOR);
  REQUIRE(arena.actors.target[raider] == arena.actors.slots.handleAt(ally));
}

TEST_CASE("an actor forgets an actor target that leaves the game") {
  ActorArena arena;
  // A player near, so the guard is in the near tier and perceives every
  // tick.
  arena.addPlayer({-6.0F, 0.0F});
  const uint32_t guard =
      arena.addActor(sentry(BehaviorTargets::OPPONENTS),
                     {.at = {0.0F, 0.0F, 0.0F}, .faction = Faction::FRIENDLY});
  const uint32_t raider =
      arena.addActor(sentry(BehaviorTargets::PLAYERS), raiderAt(4.0F, 0.0F));
  arena.step();
  REQUIRE(arena.actors.remembers_target[guard] == 1);

  (void)arena.actors.slots.destroy(arena.actors.slots.handleAt(raider));
  eng::game::compactActors(arena.actors);
  arena.step();
  REQUIRE(arena.actors.remembers_target[guard] == 0);
}

TEST_CASE("an actor far from every player perceives only on its slot's tick") {
  ActorArena arena;
  arena.addPlayer({18.0F, 0.0F});
  arena.addActor(sentry(BehaviorTargets::PLAYERS), raiderAt(-18.0F, 5.0F));
  arena.addActor(sentry(BehaviorTargets::PLAYERS), raiderAt(-18.0F, -5.0F));
  BehaviorDefinition far_sighted = sentry(BehaviorTargets::PLAYERS);
  far_sighted.senses.sight_range = 50.0F;
  // The third actor spawned: slot 2, which perceives when tick + 2 is a
  // multiple of six.
  const uint32_t watcher = arena.addActor(far_sighted, raiderAt(-18.0F, 0.0F));

  arena.step(4);
  REQUIRE(arena.actors.remembers_target[watcher] == 0);
  arena.step();
  REQUIRE(arena.actors.remembers_target[watcher] == 1);
  REQUIRE(arena.actors.last_seen_tick[watcher] == 4);
}

TEST_CASE("an actor pursuing another stops touching it, not shoving it") {
  ActorArena arena;
  arena.addPlayer({-6.0F, 6.0F});
  BehaviorDefinition hunter = eng::game::resolveBehavior({}, "defender");
  const uint32_t guard = arena.addActor(hunter, {.at = {-3.0F, 0.0F, 0.0F},
                                                 .faction = Faction::FRIENDLY,
                                                 .radius = 0.5F});
  const uint32_t raider = arena.addActor(
      sentry(BehaviorTargets::PLAYERS),
      {.at = {1.0F, 0.0F, 0.0F}, .faction = Faction::HOSTILE, .radius = 0.5F});
  arena.step(240);

  REQUIRE(arena.stateOf(guard) == "pursue");
  // The raider stands where it was: nothing pushed it off its spot.
  REQUIRE(eng::Vec2::distance(arena.actorAt(raider), {1.0F, 0.0F}) < 0.05F);
  REQUIRE(eng::Vec2::distance(arena.actorAt(guard), arena.actorAt(raider)) <
          1.2F);
}

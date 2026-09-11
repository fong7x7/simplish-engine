#include "support/actor-arena.h"

#include <catch2/catch_test_macros.hpp>
#include <game/content/behavior-lookup.h>

using eng::game::BehaviorAction;
using eng::game::BehaviorCondition;
using eng::game::BehaviorDefinition;
using eng::game::BehaviorExit;
using eng::game::defaultBehaviorState;
using eng::game::GameContent;
using eng::game::resolveBehavior;
using eng::game::test::ActorArena;

namespace {

/// A behavior of two holding states, `a` and `b`, with @p exits out of `a`.
BehaviorDefinition twoStates(std::vector<BehaviorExit> exits) {
  BehaviorDefinition behavior;
  behavior.id = "two";
  for (const char* id : {"a", "b"}) {
    behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
    behavior.states.back().id = id;
  }
  behavior.states[0].exits = std::move(exits);
  return behavior;
}

}  // namespace

TEST_CASE("a guard gives chase when it sees a player") {
  ActorArena arena;
  arena.addPlayer({4.0F, 0.0F});
  const uint32_t guard =
      arena.addActor(resolveBehavior({}, "guard"), {.at = {0.0F, 0.0F, 0.0F}});
  REQUIRE(arena.stateOf(guard) == "watch");
  arena.step();
  REQUIRE(arena.stateOf(guard) == "pursue");
}

TEST_CASE("a timed exit is taken on the tick its time is up, not before") {
  ActorArena arena;
  const uint32_t actor = arena.addActor(
      twoStates(
          {{.when = BehaviorCondition::IN_STATE_FOR, .ticks = 10, .to = 1}}),
      {});
  arena.step(10);
  REQUIRE(arena.stateOf(actor) == "a");
  arena.step();
  REQUIRE(arena.stateOf(actor) == "b");
  REQUIRE(arena.actors.state_since[actor] == 10);
}

TEST_CASE("interrupts are tested before a state's own exits") {
  BehaviorDefinition behavior =
      twoStates({{.when = BehaviorCondition::ALWAYS, .to = 1}});
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
  behavior.states.back().id = "c";
  behavior.interrupts.push_back({.when = BehaviorCondition::ALWAYS, .to = 2});
  ActorArena arena;
  const uint32_t actor = arena.addActor(behavior, {});
  arena.step();
  REQUIRE(arena.stateOf(actor) == "c");
  // An interrupt leading where the actor already is does not restart it.
  arena.step(5);
  REQUIRE(arena.actors.state_since[actor] == 0);
}

TEST_CASE(
    "a chance of a thousand in a thousand is certain, and of none never") {
  ActorArena arena;
  const uint32_t sure = arena.addActor(
      twoStates(
          {{.when = BehaviorCondition::CHANCE, .permille = 1000, .to = 1}}),
      {});
  const uint32_t never = arena.addActor(
      twoStates({{.when = BehaviorCondition::CHANCE, .permille = 0, .to = 1}}),
      {});
  arena.step(100);
  REQUIRE(arena.stateOf(sure) == "b");
  REQUIRE(arena.stateOf(never) == "a");
}

TEST_CASE("an actor that never perceived anyone has lost its target") {
  ActorArena arena;
  const uint32_t actor =
      arena.addActor(twoStates({{.when = BehaviorCondition::LOST_TARGET_FOR,
                                 .ticks = 9999,
                                 .to = 1}}),
                     {});
  arena.step();
  REQUIRE(arena.stateOf(actor) == "b");
}

TEST_CASE("the first exit that holds is the one taken") {
  BehaviorDefinition behavior =
      twoStates({{.when = BehaviorCondition::ALWAYS, .to = 1},
                 {.when = BehaviorCondition::ALWAYS, .to = 2}});
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::HOLD));
  behavior.states.back().id = "c";
  ActorArena arena;
  const uint32_t actor = arena.addActor(behavior, {});
  arena.step();
  REQUIRE(arena.stateOf(actor) == "b");
}

#include <catch2/catch_test_macros.hpp>
#include <game/content/behavior-lookup.h>
#include <string>

using namespace eng::game;

namespace {

/// A behavior called @p id with one idle state and @p exits out of it.
BehaviorDefinition oneState(std::string id, std::vector<BehaviorExit> exits) {
  BehaviorDefinition behavior;
  behavior.id = std::move(id);
  behavior.states.push_back(defaultBehaviorState(BehaviorAction::IDLE));
  behavior.states.back().exits = std::move(exits);
  return behavior;
}

}  // namespace

TEST_CASE("every built-in behavior is well formed and uniquely named") {
  const auto presets = builtInBehaviors();
  REQUIRE(presets.size() == 9);
  REQUIRE(presets.front().id == "idle");
  for (size_t i = 0; i < presets.size(); ++i) {
    INFO(presets[i].id);
    REQUIRE(behaviorIsWellFormed(presets[i]));
    REQUIRE_FALSE(presets[i].name.empty());
    for (size_t j = i + 1; j < presets.size(); ++j) {
      REQUIRE(presets[i].id != presets[j].id);
    }
  }
}

TEST_CASE("a behavior resolves to the project's own before a built-in") {
  GameContent content;
  content.behaviors.push_back(oneState("guard", {}));
  content.behaviors.back().name = "Project Guard";

  REQUIRE(resolveBehavior(content, "guard").name == "Project Guard");
  REQUIRE(resolveBehavior(content, "chase").id == "chase");
}

TEST_CASE("an unknown or broken behavior resolves to idle") {
  GameContent content;
  content.behaviors.push_back(oneState("broken", {{.to = 3}}));
  content.behaviors.push_back(BehaviorDefinition{.id = "empty"});

  REQUIRE(resolveBehavior(content, "nobody").id == "idle");
  REQUIRE(resolveBehavior(content, "").id == "idle");
  REQUIRE(resolveBehavior(content, "broken").id == "idle");
  REQUIRE(resolveBehavior(content, "empty").id == "idle");
}

TEST_CASE("a behavior is well formed only when every index is in range") {
  BehaviorDefinition behavior = oneState("ok", {{.to = 0}});
  REQUIRE(behaviorIsWellFormed(behavior));
  behavior.initial = 1;
  REQUIRE_FALSE(behaviorIsWellFormed(behavior));
  behavior.initial = 0;
  behavior.interrupts.push_back({.to = 2});
  REQUIRE_FALSE(behaviorIsWellFormed(behavior));
}

TEST_CASE("a default state carries its action's distances") {
  REQUIRE(defaultBehaviorState(BehaviorAction::PURSUE).near_tiles == 0.9F);
  REQUIRE(defaultBehaviorState(BehaviorAction::KEEP_DISTANCE).far_tiles ==
          6.0F);
  REQUIRE(defaultBehaviorState(BehaviorAction::WANDER).far_tiles == 4.0F);
  REQUIRE(defaultBehaviorState(BehaviorAction::HOLD).speed_permille == 1000);
}

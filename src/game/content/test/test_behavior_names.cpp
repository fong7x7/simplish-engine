#include <catch2/catch_test_macros.hpp>
#include <game/content/behavior-names.h>

using namespace eng::game;

TEST_CASE("every action's name reads back as that action") {
  for (int i = 0; i <= static_cast<int>(BehaviorAction::CHARGE); ++i) {
    const auto action = static_cast<BehaviorAction>(i);
    REQUIRE(parseBehaviorAction(behaviorActionName(action)) == action);
  }
  REQUIRE(behaviorActionName(BehaviorAction::KEEP_DISTANCE) == "keep_distance");
}

TEST_CASE("every condition's name reads back as that condition") {
  for (int i = 0; i <= static_cast<int>(BehaviorCondition::CHANCE); ++i) {
    const auto condition = static_cast<BehaviorCondition>(i);
    REQUIRE(parseBehaviorCondition(behaviorConditionName(condition)) ==
            condition);
  }
  REQUIRE(behaviorConditionName(BehaviorCondition::LOST_TARGET_FOR) ==
          "lost_target_for");
}

TEST_CASE("facings and factions read back by name") {
  REQUIRE(parseBehaviorFacing("locked") == BehaviorFacing::LOCKED);
  REQUIRE(behaviorFacingName(BehaviorFacing::TARGET) == "target");
  for (const Faction faction : ALL_FACTIONS) {
    REQUIRE(parseFaction(factionName(faction)) == faction);
  }
}

TEST_CASE("a word that names nothing reads as nothing") {
  REQUIRE_FALSE(parseBehaviorAction("dance").has_value());
  REQUIRE_FALSE(parseBehaviorCondition("").has_value());
  REQUIRE_FALSE(parseBehaviorFacing("Target").has_value());
  REQUIRE_FALSE(parseFaction("enemy").has_value());
}

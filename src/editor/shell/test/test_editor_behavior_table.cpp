#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-behavior-table.h>
#include <filesystem>
#include <game/content/behavior-lookup.h>
#include <string>

using namespace eng;
using namespace eng::editor;

namespace {

/// A behaviors table whose entries are @p entries, a JSON array.
std::string table(const std::string& entries) {
  return R"({"schema": "simplish/data_table/1.0", "id": "behaviors",
             "content": {"entry_schema": "simplish/behavior/1.0",
                         "entries": )" +
         entries + "}}";
}

/// Whether @p problems has a line containing @p text.
bool mentions(const std::vector<std::string>& problems,
              const std::string& text) {
  for (const std::string& problem : problems) {
    if (problem.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

/// A sentry: watches, then pursues, stopping a tile and a half short.
constexpr const char* SENTRY = R"([{
  "id": "sentry", "name": "Sentry",
  "senses": {"sight_range": 12, "view_degrees": 120, "memory_ticks": 90},
  "movement": {"speed": 2.5, "turn_degrees_per_second": 180},
  "initial": "watch",
  "interrupts": [{"when": "blocked", "to": "watch"}],
  "states": [
    {"id": "pursue", "do": "pursue", "stop_within": 1.5, "speed_permille": 1500,
     "exits": [{"when": "lost_target_for", "ticks": 120, "to": "watch"}]},
    {"id": "watch", "do": "idle", "face": "target", "clip": "look",
     "exits": [{"when": "sees_target", "to": "pursue"},
               {"when": "chance", "permille": 5, "to": "pursue"}]}
  ]}])";

}  // namespace

TEST_CASE("a behaviors table reads every part of a behavior") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(SENTRY));
  REQUIRE(read.problems.empty());
  REQUIRE(read.behaviors.size() == 1);
  const game::BehaviorDefinition& sentry = read.behaviors[0];

  REQUIRE(sentry.name == "Sentry");
  REQUIRE(sentry.senses.sight_range == 12.0F);
  REQUIRE(sentry.senses.view_degrees == 120.0F);
  REQUIRE(sentry.senses.memory_ticks == 90);
  REQUIRE(sentry.movement.speed == 2.5F);
  REQUIRE(sentry.initial == 1);
  REQUIRE(sentry.interrupts.size() == 1);
  REQUIRE(sentry.states[0].near_tiles == 1.5F);
  REQUIRE(sentry.states[0].speed_permille == 1500);
  REQUIRE(sentry.states[0].exits[0].ticks == 120);
  REQUIRE(sentry.states[0].exits[0].to == 1);
  REQUIRE(sentry.states[1].facing == game::BehaviorFacing::TARGET);
  REQUIRE(sentry.states[1].clip == "look");
  REQUIRE(sentry.states[1].exits[1].permille == 5);
  REQUIRE(game::behaviorIsWellFormed(sentry));
}

TEST_CASE("each action reads its distances under its own keys") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(R"([{
    "id": "mix", "states": [
      {"id": "a", "do": "keep_distance", "min": 2, "max": 9},
      {"id": "b", "do": "wander", "radius": 3},
      {"id": "c", "do": "flee", "distance": 11},
      {"id": "d", "do": "follow"}]}])"));
  const auto& states = read.behaviors.at(0).states;
  REQUIRE(states[0].near_tiles == 2.0F);
  REQUIRE(states[0].far_tiles == 9.0F);
  REQUIRE(states[1].far_tiles == 3.0F);
  REQUIRE(states[2].far_tiles == 11.0F);
  // Absent, an action keeps its default.
  REQUIRE(states[3].near_tiles == 2.0F);
}

TEST_CASE("rows and states that cannot be used are skipped and said so") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(R"([
    {"id": "Bad Id", "states": [{"id": "a"}]},
    {"id": "empty", "states": []},
    {"id": "ok", "states": [{"id": "a"}, {"id": "a"}, {"name": "no id"}]},
    {"id": "ok", "states": [{"id": "b"}]}])"));

  REQUIRE(read.behaviors.size() == 1);
  REQUIRE(read.behaviors[0].states.size() == 1);
  REQUIRE(mentions(read.problems, "not an id"));
  REQUIRE(mentions(read.problems, "empty: a behavior with no usable states"));
  REQUIRE(mentions(read.problems, "ok: a: a second state"));
  REQUIRE(mentions(read.problems, "ok: a second row"));
}

TEST_CASE("words the format does not know fall back, and exits to nowhere go") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(R"([{
    "id": "odd", "initial": "nowhere", "states": [
      {"id": "a", "do": "dance", "face": "sideways",
       "exits": [{"when": "feels_like_it", "to": "a"},
                 {"when": "always", "to": "b"},
                 {"when": "always", "to": "missing"}]},
      {"id": "b"}]}])"));
  const game::BehaviorDefinition& odd = read.behaviors.at(0);

  REQUIRE(odd.states[0].action == game::BehaviorAction::IDLE);
  REQUIRE(odd.states[0].facing == game::BehaviorFacing::MOVEMENT);
  REQUIRE(odd.states[0].exits.size() == 1);
  REQUIRE(odd.initial == 0);
  REQUIRE(mentions(read.problems, "\"dance\" is not an action"));
  REQUIRE(mentions(read.problems, "\"sideways\" is not a facing"));
  REQUIRE(mentions(read.problems, "feels_like_it"));
  REQUIRE(mentions(read.problems, "initial \"nowhere\""));
  REQUIRE(game::behaviorIsWellFormed(odd));
}

TEST_CASE("numbers out of range are held to it, and not-numbers default") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(R"([{
    "id": "wild", "senses": {"sight_range": -4, "view_degrees": "wide"},
    "movement": {"speed": 900}, "states": [{"id": "a"}]}])"));
  const game::BehaviorDefinition& wild = read.behaviors.at(0);
  REQUIRE(wild.senses.sight_range == 0.0F);
  REQUIRE(wild.senses.view_degrees == game::BehaviorSenses{}.view_degrees);
  REQUIRE(wild.movement.speed == 20.0F);
  REQUIRE(mentions(read.problems, "view_degrees is not a number"));
  REQUIRE(mentions(read.problems, "speed was held"));
}

TEST_CASE("a file that is not a behaviors table gives nothing, and says so") {
  REQUIRE(parseEditorBehaviorTable("{").behaviors.empty());
  const EditorBehaviorTable characters = parseEditorBehaviorTable(
      R"({"schema": "simplish/data_table/1.0",
          "content": {"entry_schema": "simplish/character/1.0", "entries": []}})");
  REQUIRE(characters.behaviors.empty());
  REQUIRE(mentions(characters.problems, "not a behaviors table"));
}

TEST_CASE("a project with no behaviors table has none, and no problems") {
  const auto root =
      std::filesystem::temp_directory_path() / "simplish-no-behaviors-table";
  const EditorBehaviorTable read = loadEditorBehaviorTable(root);
  REQUIRE(read.behaviors.empty());
  REQUIRE(read.problems.empty());
  REQUIRE(editorBehaviorTablePath(root).filename() == "behaviors.data.json");
}

TEST_CASE("whom a behavior targets is read, and a word for nobody is players") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(R"([
    {"id": "defender", "senses": {"targets": "opponents"},
     "states": [{"id": "a"}]},
    {"id": "confused", "senses": {"targets": "everyone"},
     "states": [{"id": "a"}]},
    {"id": "plain", "states": [{"id": "a"}]}])"));

  REQUIRE(read.behaviors.at(0).senses.targets ==
          game::BehaviorTargets::OPPONENTS);
  REQUIRE(read.behaviors.at(1).senses.targets ==
          game::BehaviorTargets::PLAYERS);
  REQUIRE(read.behaviors.at(2).senses.targets ==
          game::BehaviorTargets::PLAYERS);
  REQUIRE(mentions(read.problems, "\"everyone\" is not whom to target"));
}

TEST_CASE("an attacking state reads its attack under its action's keys") {
  const EditorBehaviorTable read = parseEditorBehaviorTable(table(R"([
    {"id": "brute", "states": [
      {"id": "bite", "do": "melee", "damage": 2, "reach": 0.5,
       "cooldown_ticks": 30,
       "exits": [{"when": "health_below", "permille": 300, "to": "shoot"}]},
      {"id": "shoot", "do": "fire", "count": 5, "spread_degrees": 40,
       "projectile_speed": 12,
       "exits": [{"when": "damaged", "ticks": 10, "to": "spit"}]},
      {"id": "spit", "do": "spit", "radius": 1.5, "duration_ticks": 90,
       "exits": [{"when": "allies_within", "tiles": 3, "to": "burst"}]},
      {"id": "burst", "do": "detonate", "radius": 3, "damage": 4}]}])"));

  REQUIRE(read.problems.empty());
  const auto& states = read.behaviors.at(0).states;
  REQUIRE(states[0].attack.damage == 2);
  REQUIRE(states[0].attack.reach_tiles == 0.5F);
  REQUIRE(states[0].attack.cooldown_ticks == 30);
  REQUIRE(states[0].exits[0].when == game::BehaviorCondition::HEALTH_BELOW);
  REQUIRE(states[1].attack.count == 5);
  REQUIRE(states[1].attack.speed == 12.0F);
  REQUIRE(states[1].attack.damage == 1);
  REQUIRE(states[1].exits[0].when == game::BehaviorCondition::DAMAGED);
  REQUIRE(states[2].attack.duration_ticks == 90);
  REQUIRE(states[2].exits[0].when == game::BehaviorCondition::ALLIES_WITHIN);
  REQUIRE(states[3].attack.radius == 3.0F);
  REQUIRE(states[3].attack.damage == 4);
}

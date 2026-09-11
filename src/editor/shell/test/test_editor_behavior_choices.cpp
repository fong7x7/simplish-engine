#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-behavior-choices.h>
#include <game/content/behavior-lookup.h>

using namespace eng;
using namespace eng::editor;

namespace {

/// A project behavior called @p id, named @p name, with one idle state.
game::BehaviorDefinition own(std::string id, std::string name) {
  game::BehaviorDefinition behavior;
  behavior.id = std::move(id);
  behavior.name = std::move(name);
  behavior.states.push_back(
      game::defaultBehaviorState(game::BehaviorAction::IDLE));
  return behavior;
}

}  // namespace

TEST_CASE("the Behavior row offers none, then every built-in behavior") {
  const EditorBehaviorChoices choices = editorBehaviorChoices({}, "");
  REQUIRE(choices.names.front() == "None");
  REQUIRE(choices.refs.front().empty());
  REQUIRE(choices.names.size() == game::builtInBehaviors().size() + 1);
  REQUIRE(choices.refs[3] == "behavior:guard");
  REQUIRE(choices.current == 0);
}

TEST_CASE("a project behavior replaces its built-in in place, or comes last") {
  const std::vector<game::BehaviorDefinition> project{
      own("zombie", "Zombie"), own("guard", "Night Guard")};
  const EditorBehaviorChoices choices =
      editorBehaviorChoices(project, "behavior:guard");

  REQUIRE(choices.names[3] == "Night Guard");
  REQUIRE(choices.current == 3);
  REQUIRE(choices.names.back() == "Zombie");
  REQUIRE(choices.refs.back() == "behavior:zombie");
  REQUIRE(findEditorBehavior(project, "behavior:guard")->name == "Night Guard");
}

TEST_CASE("a behavior the project no longer has is offered as missing") {
  const EditorBehaviorChoices choices =
      editorBehaviorChoices({}, "behavior:gone");
  REQUIRE(choices.names.back() == "behavior:gone (missing)");
  REQUIRE(choices.current == choices.names.size() - 1);
  REQUIRE(findEditorBehavior({}, "behavior:gone") == nullptr);
}

TEST_CASE("behavior references read both ways") {
  REQUIRE(editorBehaviorRef("guard") == "behavior:guard");
  REQUIRE(editorBehaviorIdOf("behavior:guard") == "guard");
  REQUIRE(editorBehaviorIdOf("character:guard").empty());
  REQUIRE(editorBehaviorIdOf("").empty());
}

TEST_CASE("the Faction row offers the three sides in order") {
  REQUIRE(editorFactionNames() ==
          std::vector<std::string>{"Hostile", "Neutral", "Friendly"});
  REQUIRE(editorFactionName(game::Faction::FRIENDLY) == "Friendly");
}

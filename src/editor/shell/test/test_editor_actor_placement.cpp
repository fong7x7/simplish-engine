#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-actor-placement.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// A placement of nothing measured at (3, 4), turned @p degrees about Z.
EditorPlacement propAt(float degrees, std::string behavior = "behavior:guard") {
  EditorPlacement placement;
  placement.id = "knight_01";
  placement.position = {3.0f, 4.0f, 0.0f};
  placement.rotation = {0.0f, 0.0f, degrees};
  placement.behavior = std::move(behavior);
  return placement;
}

}  // namespace

TEST_CASE("only a prop with a behavior is an actor, in document order") {
  EditorDocument document;
  document.placements = {propAt(0, ""), propAt(0), propAt(0, ""), propAt(0)};
  document.placements[3].id = "knight_02";

  REQUIRE_FALSE(isEditorActor(document.placements[0]));
  REQUIRE(editorActorPlacements(document) == std::vector<size_t>{1, 3});
  REQUIRE(editorActorIds(document) ==
          std::vector<std::string>{"knight_01", "knight_02"});
}

TEST_CASE("an unturned prop faces -Y, as a model's front does") {
  REQUIRE(editorActorYawDegrees(propAt(0)) == -90.0f);
  REQUIRE(editorActorYawDegrees(propAt(90)) == 0.0f);
  const Vec2 facing = editorActorFacing(propAt(0));
  REQUIRE(facing.x == Approx(0.0f).margin(1e-6));
  REQUIRE(facing.y == Approx(-1.0f));
}

TEST_CASE("an actor spawns on the middle of its tile, as wide as its model") {
  EditorPlacement placement = propAt(45);
  placement.faction = game::Faction::FRIENDLY;
  const game::ActorSpawn spawn = editorActorSpawn(placement, EditorAsset{});

  REQUIRE(spawn.at.x == 3.5f);
  REQUIRE(spawn.at.y == 4.5f);
  REQUIRE(spawn.yaw_degrees == -45.0f);
  REQUIRE(spawn.behavior == "guard");
  REQUIRE(spawn.faction == game::Faction::FRIENDLY);
  // An unmeasured asset is the unit box: half a tile either side, turned
  // or not.
  REQUIRE(spawn.radius == Approx(0.5f));
  REQUIRE(spawn.height == Approx(1.0f));
}

TEST_CASE("an actor is drawn where the game has it, facing its way") {
  const EditorPlacement placement = propAt(0);
  const EditorPlacement posed =
      editorActorPose(placement, {7.5f, 1.5f, 0.0f}, {1.0f, 0.0f});
  REQUIRE(posed.position.x == 7.0f);
  REQUIRE(posed.position.y == 1.0f);
  REQUIRE(posed.rotation.z == Approx(90.0f));
  REQUIRE(editorActorFacing(posed).x == Approx(1.0f));
  // A zero facing keeps the heading it was placed with.
  REQUIRE(editorActorPose(placement, {7.5f, 1.5f, 0.0f}, {}).rotation.z ==
          0.0f);
}

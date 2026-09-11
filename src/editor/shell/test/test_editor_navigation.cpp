#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-nav-overlay.h>
#include <editor/shell/editor-navigation.h>
#include <editor/shell/editor-path-query.h>
#include <editor/shell/editor-player-start-ops.h>

using Catch::Approx;
using namespace eng;
using namespace eng::editor;

namespace {

/// A crate (an unmeasured asset: the unit box on its tile) at @p x, @p y.
EditorPlacement crate(std::string id, float x, float y) {
  return {std::move(id), 0, {x, y, 0.0f}, {}};
}

/// A room of crates, walls along x = 3 and x = 7 and y = 3 and y = 7, closed
/// all round, with player 1's start outside at (0.5, 0.5).
EditorDocument closedRoom() {
  EditorDocument document;
  document.player_starts.push_back(makeEditorPlayerStart(1, {0.5f, 0.5f, 0}));
  int n = 0;
  for (int i = 3; i <= 7; ++i) {
    const auto f = static_cast<float>(i);
    document.placements.push_back(crate("w" + std::to_string(n++), f, 3.0f));
    document.placements.push_back(crate("w" + std::to_string(n++), f, 7.0f));
    document.placements.push_back(crate("w" + std::to_string(n++), 3.0f, f));
    document.placements.push_back(crate("w" + std::to_string(n++), 7.0f, f));
  }
  return document;
}

/// @p document with an actor inside the room, at (5, 5).
EditorDocument roomWithActor(EditorDocument document) {
  EditorPlacement knight = crate("knight_01", 5.0f, 5.0f);
  knight.behavior = "behavior:chase";
  document.placements.push_back(knight);
  return document;
}

/// How many of @p navigation's cells are @p kind.
long cellsOf(const EditorNavigation& navigation, EditorNavCell kind) {
  return std::ranges::count(navigation.cells, kind);
}

}  // namespace

TEST_CASE(
    "navigation sorts every cell into solid, narrow, open or unreachable") {
  const EditorNavigation navigation = analyseEditorNavigation(closedRoom(), {});
  REQUIRE(navigation.reachability_known);
  REQUIRE(navigation.clearance == 2);
  REQUIRE(navigation.cells.size() == navigation.grid.cellCount());
  REQUIRE(cellsOf(navigation, EditorNavCell::SOLID) > 0);
  REQUIRE(cellsOf(navigation, EditorNavCell::NARROW) > 0);
  // The inside of the room is floor no player start reaches.
  REQUIRE(cellsOf(navigation, EditorNavCell::UNREACHABLE) > 0);
  const auto inside = navigation.grid.cellAt({5.5f, 5.5f});
  REQUIRE(navigation.cells[navigation.grid.indexOf(*inside)] ==
          EditorNavCell::UNREACHABLE);
}

TEST_CASE("an actor walled in is reported, and one outside is not") {
  const EditorNavigation walled =
      analyseEditorNavigation(roomWithActor(closedRoom()), {});
  REQUIRE(walled.unreachable_actors == std::vector<std::string>{"knight_01"});

  EditorDocument open = closedRoom();
  EditorPlacement knight = crate("knight_01", 1.0f, 5.0f);
  knight.behavior = "behavior:chase";
  open.placements.push_back(knight);
  REQUIRE(analyseEditorNavigation(open, {}).unreachable_actors.empty());
}

TEST_CASE("an actor inside a prop has nowhere to stand") {
  EditorDocument document = closedRoom();
  EditorPlacement stuck = crate("stuck_01", 5.0f, 5.0f);
  stuck.behavior = "behavior:idle";
  document.placements.push_back(stuck);
  // A slab filling the room over it: the actor is a prop with a behavior,
  // so not a box itself, but the slab is.
  EditorPlacement slab = crate("slab_01", 5.0f, 5.0f);
  slab.scale = 4.0f;
  document.placements.push_back(slab);
  const EditorNavigation navigation = analyseEditorNavigation(document, {});
  REQUIRE(navigation.stranded_actors == std::vector<std::string>{"stuck_01"});
}

TEST_CASE("with no player start nothing is called unreachable") {
  EditorDocument document = roomWithActor(closedRoom());
  document.player_starts.clear();
  const EditorNavigation navigation = analyseEditorNavigation(document, {});
  REQUIRE_FALSE(navigation.reachability_known);
  REQUIRE(cellsOf(navigation, EditorNavCell::UNREACHABLE) == 0);
  REQUIRE(navigation.unreachable_actors.empty());
}

TEST_CASE("a path round a wall is found and measured, one through it is not") {
  const EditorNavigation navigation = analyseEditorNavigation(closedRoom(), {});
  const EditorPathAnswer around =
      findEditorPath(navigation, {{0.5f, 0.5f}, {9.5f, 9.5f}, 0.3f});
  REQUIRE(around.status == spatial::PathStatus::FOUND);
  REQUIRE_FALSE(around.waypoints.empty());
  REQUIRE(around.waypoints.back().x == Approx(9.625f));
  REQUIRE(around.length_tiles > 12.0f);

  const EditorPathAnswer in =
      findEditorPath(navigation, {{0.5f, 0.5f}, {5.5f, 5.5f}, 0.3f});
  REQUIRE(in.status == spatial::PathStatus::UNREACHABLE);
  REQUIRE(in.waypoints.empty());
}

TEST_CASE("the overlay draws every cell that is not open, as runs") {
  const EditorNavigation navigation = analyseEditorNavigation(closedRoom(), {});
  const EditorNavOverlay overlay = editorNavOverlay(navigation);
  REQUIRE(overlay.cell_size == navigation.grid.spec().cell_size);
  size_t drawn = 0;
  for (const EditorNavRun& run : overlay.runs) {
    REQUIRE(run.kind != EditorNavCell::OPEN);
    REQUIRE(run.end > run.first);
    drawn += run.end - run.first;
  }
  const auto open =
      static_cast<size_t>(cellsOf(navigation, EditorNavCell::OPEN));
  REQUIRE(drawn + open == navigation.cells.size());
}

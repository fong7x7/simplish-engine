#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-tree-view.h>

using namespace eng;

namespace {

GuiTreeView levels() {
  GuiTreeView view;
  view.rect = {0, 0, 300, 400};
  view.row_height = 20;
  view.roots = {{"Levels", {{"Station"}, {"Docks", {{"Pier"}}}}, true},
                {"Sounds"}};
  view.refresh();
  return view;
}

}  // namespace

TEST_CASE("a tree view shows expanded nodes' children as rows") {
  GuiTreeView view = levels();
  REQUIRE(view.row_count == 4);  // Levels, Station, Docks, Sounds
  CHECK(view.nodeAt(2).label == "Docks");
  CHECK(view.pathOf(2) == std::vector<size_t>{0, 1});
}

TEST_CASE("clicking an arrow folds and unfolds") {
  GuiTreeView view = levels();
  // Docks is at depth 1: its arrow is after one indent.
  view.handleClick({.x = 6 + 16 + 2, .y = 2 * 20 + 5});
  CHECK(view.row_count == 5);
  CHECK(view.nodeAt(3).label == "Pier");
  view.handleClick({.x = 8, .y = 5});  // fold Levels
  CHECK(view.row_count == 2);
}

TEST_CASE("RIGHT unfolds and steps in; LEFT folds and steps out") {
  GuiTreeView view = levels();
  view.setCurrent(2);                    // Docks
  view.handleNav(GuiNavCommand::RIGHT);  // unfold
  CHECK(view.nodeAt(2).expanded);
  view.handleNav(GuiNavCommand::RIGHT);  // step into Pier
  CHECK(view.selected == std::vector<size_t>{3});
  view.handleNav(GuiNavCommand::LEFT);  // out to Docks
  CHECK(view.selected == std::vector<size_t>{2});
  view.handleNav(GuiNavCommand::LEFT);  // fold Docks
  CHECK_FALSE(view.nodeAt(2).expanded);
}

#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-table.h>

using namespace eng;

namespace {

GuiTable table() {
  GuiTable t;
  t.rect = {0, 0, 300, 200};
  t.row_height = 20;
  t.row_count = 3;
  t.columns = {{"Name", 150.0f}, {"Health", 80.0f, Align::END}};
  return t;
}

}  // namespace

TEST_CASE("a header click sorts, and a second reverses it") {
  GuiTable t = table();
  std::vector<std::pair<size_t, GuiSortDirection>> asked;
  t.on_sort = [&](size_t c, GuiSortDirection d) {
    asked.emplace_back(c, d);
  };
  t.handleClick({.x = 160, .y = 5});
  t.handleClick({.x = 160, .y = 5});
  REQUIRE(asked.size() == 2);
  CHECK(asked[0] == std::pair{size_t{1}, GuiSortDirection::ASCENDING});
  CHECK(asked[1] == std::pair{size_t{1}, GuiSortDirection::DESCENDING});
  CHECK(t.sort_column == 1);
}

TEST_CASE("dragging a header's right edge resizes its column") {
  GuiTable t = table();
  CHECK(t.handleMouseDown({.x = 151, .y = 5}));
  t.handleMouseMove({.x = 200, .y = 5});
  t.handleMouseUp({});
  CHECK(t.columns[0].width == 200.0f);
  t.handleMouseDown({.x = 200, .y = 5});
  t.handleMouseMove({.x = 5, .y = 5});
  CHECK(t.columns[0].width == t.min_column_width);
}

TEST_CASE("rows sit below the header") {
  GuiTable t = table();
  CHECK(t.bodyRect().y == 20.0f);
  CHECK(t.rowAt(10, 25) == 0);
  CHECK(t.columnAt(200) == 1);
  t.handleClick({.x = 10, .y = 45});
  CHECK(t.selected == std::vector<size_t>{1});
}

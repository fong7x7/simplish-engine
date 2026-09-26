#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-virtual-list.h>

using namespace eng;

namespace {

/// A 100-pixel-tall view onto ten thousand 20-pixel rows.
GuiVirtualList bigList() {
  GuiVirtualList list;
  list.rect = {0, 0, 200, 100};
  list.row_count = 10000;
  list.row_height = 20;
  return list;
}

}  // namespace

TEST_CASE("a virtual list draws only the rows in view") {
  GuiVirtualList list = bigList();
  std::vector<size_t> drawn;
  list.draw_row = [&](const GuiDrawContext&, const GuiListRow& row) {
    drawn.push_back(row.index);
  };
  list.scrollBy(0, 1000);
  GuiRendererContext renderer;
  renderer.beginFrame();
  GuiDrawContext ctx;
  ctx.renderer = &renderer;
  list.render(ctx);
  REQUIRE_FALSE(drawn.empty());
  CHECK(drawn.front() == 50);
  CHECK(drawn.size() <= 7);
}

TEST_CASE("a click selects; shift extends when several may be selected") {
  GuiVirtualList list = bigList();
  list.selection_mode = GuiListSelection::MULTIPLE;
  list.handleClick({.x = 10, .y = 25});
  CHECK(list.selected == std::vector<size_t>{1});
  list.handleClick({.x = 10, .y = 85, .shift_held = true});
  CHECK(list.selected == std::vector<size_t>{1, 2, 3, 4});
  CHECK(list.isSelected(3));
}

TEST_CASE("UP and DOWN move the current row and keep it in view") {
  GuiVirtualList list = bigList();
  for (int i = 0; i < 8; ++i) {
    list.handleNav(GuiNavCommand::DOWN);
  }
  CHECK(list.selected == std::vector<size_t>{8});
  CHECK(list.scrollOffset() == 80.0f);  // row 8's bottom at the view's
  size_t activated = 0;
  list.on_activate = [&](size_t i) {
    activated = i;
  };
  list.handleNav(GuiNavCommand::CONFIRM);
  CHECK(activated == 8);
}

TEST_CASE("a double-click activates the row") {
  GuiVirtualList list = bigList();
  long activated = -1;
  list.on_activate = [&](size_t i) {
    activated = static_cast<long>(i);
  };
  list.handleClick({.type = GuiMouseEventType::DOUBLE_CLICK, .x = 10, .y = 45});
  CHECK(activated == 2);
}

TEST_CASE("the wheel scrolls within the rows") {
  GuiVirtualList list = bigList();
  list.handleScroll({.delta_y = 1.0f});
  CHECK(list.scrollOffset() == 0.0f);
  list.handleScroll({.delta_y = -1.0f});
  CHECK(list.scrollOffset() == 48.0f);
  list.scrollBy(0, 1e9f);
  CHECK(list.scrollOffset() == 10000.0f * 20.0f - 100.0f);
}

#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-popover-placement.h>

using namespace eng;

namespace {

constexpr Rect VIEW{0.0f, 0.0f, 800.0f, 600.0f};

}  // namespace

TEST_CASE("a popover opens below its anchor, lined up with its start") {
  const Rect r = placePopover({100, 100, 80, 30}, {200, 150}, VIEW, {});
  CHECK(r.x == 100.0f);
  CHECK(r.y == 134.0f);
  CHECK(r.w == 200.0f);
}

TEST_CASE("without room below, it flips above") {
  const Rect r = placePopover({100, 540, 80, 30}, {200, 150}, VIEW, {});
  CHECK(r.y + r.h == 536.0f);
}

TEST_CASE("it slides along to stay inside the viewport's margin") {
  const Rect r = placePopover({750, 100, 40, 30}, {200, 150}, VIEW, {});
  CHECK(r.x + r.w == 792.0f);
}

TEST_CASE("centre and end alignment, and the sides") {
  const Rect anchor{300, 300, 100, 40};
  CHECK(placePopover(anchor, {60, 20}, VIEW, {.align = Align::CENTER}).x ==
        320.0f);
  CHECK(placePopover(anchor, {60, 20}, VIEW, {.align = Align::END}).x ==
        340.0f);
  const Rect right =
      placePopover(anchor, {60, 20}, VIEW, {.side = GuiPopoverSide::RIGHT});
  CHECK(right.x == 404.0f);
  CHECK(right.y == 300.0f);
}

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-render-transform.h>

using namespace eng;
using Catch::Approx;

TEST_CASE("a transform scales about a box's centre, then moves") {
  const Rect box{10.0f, 10.0f, 20.0f, 20.0f};
  const GuiRenderTransform t = GuiRenderTransform::about(box, 0.5f, 3.0f, 0.0f);
  const Rect drawn = t.map(box);
  CHECK(drawn.x == Approx(18.0f));
  CHECK(drawn.y == Approx(15.0f));
  CHECK(drawn.w == Approx(10.0f));
}

TEST_CASE("an inner transform is applied first") {
  const GuiRenderTransform outer{2.0f, 10.0f, 0.0f};
  const GuiRenderTransform inner{1.0f, 5.0f, 0.0f};
  const GuiRenderTransform both = outer.after(inner);
  CHECK(both.mapX(1.0f) == Approx(outer.mapX(inner.mapX(1.0f))));
}

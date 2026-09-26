#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-toasts.h>

using namespace eng;

TEST_CASE("toasts show, stack, and go when their time is up") {
  GuiToasts toasts;
  toasts.rect = {0, 0, 800, 600};
  toasts.show("Saved");
  toasts.show("Build failed", GuiToastKind::ERROR, 8.0f);
  REQUIRE(toasts.shown().size() == 2);
  const GuiDrawContext ctx{};
  toasts.update(ctx, 5.0f);
  REQUIRE(toasts.shown().size() == 1);
  CHECK(toasts.shown().front().text == "Build failed");
  toasts.update(ctx, 3.0f);
  CHECK(toasts.shown().empty());
}

TEST_CASE("only the newest few are kept") {
  GuiToasts toasts;
  toasts.max_shown = 2;
  toasts.show("a");
  toasts.show("b");
  toasts.show("c");
  REQUIRE(toasts.shown().size() == 2);
  CHECK(toasts.shown().front().text == "b");
}

namespace {

/// The right edge of what @p toasts draws, shadows aside.
float drawnRightEdge(GuiToasts& toasts) {
  GuiRendererContext renderer;
  renderer.beginFrame();
  GuiDrawContext ctx;
  ctx.renderer = &renderer;
  toasts.update(ctx, 1.0f);
  toasts.render(ctx);
  float right = 0.0f;
  for (const GuiVertex& v : renderer.vertices) {
    if ((v.flags & GUI_VERTEX_SHADOW) == 0) {
      right = std::max(right, v.pos[0]);
    }
  }
  return right;
}

}  // namespace

TEST_CASE("toasts draw bottom-right and let the pointer through") {
  GuiToasts toasts;
  toasts.rect = {0, 0, 800, 600};
  CHECK(toasts.pointer_through);
  toasts.show("Saved");
  // Its box's right edge is 16 in from the corner.
  CHECK(drawnRightEdge(toasts) == 784.0f);
}

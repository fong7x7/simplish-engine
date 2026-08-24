#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-viewport-widget.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

EditorViewportWidget makeViewport() {
  EditorViewportWidget viewport;
  viewport.rect = eng::makeRect(0.0f, 64.0f, 800.0f, 600.0f);
  return viewport;
}

eng::GuiMouseEvent mouseAt(float x, float y, eng::GuiMouseButton button,
                           bool shift = false) {
  eng::GuiMouseEvent event{};
  event.x = x;
  event.y = y;
  event.button = button;
  event.shift_held = shift;
  return event;
}

}  // namespace

TEST_CASE("a middle-drag starts a pan and moves the camera") {
  EditorViewportWidget viewport = makeViewport();
  REQUIRE(viewport.handleMouseDown(
      mouseAt(400.0f, 300.0f, eng::GuiMouseButton::MIDDLE)));

  viewport.handleMouseMove(
      mouseAt(440.0f, 320.0f, eng::GuiMouseButton::MIDDLE));

  REQUIRE(viewport.camera.focus.x == Approx(-40.0f));
  REQUIRE(viewport.camera.focus.y == Approx(-20.0f));
}

TEST_CASE("shift plus left-drag pans; a plain left-drag does not") {
  EditorViewportWidget viewport = makeViewport();

  REQUIRE(viewport.handleMouseDown(
      mouseAt(100.0f, 100.0f, eng::GuiMouseButton::LEFT, /*shift=*/true)));
  viewport.handleMouseUp(mouseAt(100.0f, 100.0f, eng::GuiMouseButton::LEFT));

  // A plain left-drag is reserved for the active tool, so the viewport
  // declines to capture it.
  REQUIRE_FALSE(viewport.handleMouseDown(
      mouseAt(100.0f, 100.0f, eng::GuiMouseButton::LEFT)));
}

TEST_CASE("releasing the button ends the pan") {
  EditorViewportWidget viewport = makeViewport();
  viewport.handleMouseDown(
      mouseAt(400.0f, 300.0f, eng::GuiMouseButton::MIDDLE));
  viewport.handleMouseUp(mouseAt(400.0f, 300.0f, eng::GuiMouseButton::MIDDLE));

  const IsoPoint focus_before = viewport.camera.focus;
  viewport.handleMouseMove(
      mouseAt(700.0f, 500.0f, eng::GuiMouseButton::MIDDLE));

  REQUIRE(viewport.camera.focus.x == Approx(focus_before.x));
  REQUIRE(viewport.camera.focus.y == Approx(focus_before.y));
}

TEST_CASE("moving inside the viewport reports the hovered tile") {
  EditorViewportWidget viewport = makeViewport();
  const float cx = viewport.rect.x + viewport.rect.w * 0.5f;
  const float cy = viewport.rect.y + viewport.rect.h * 0.5f;

  viewport.handleMouseMove(mouseAt(cx, cy, eng::GuiMouseButton::LEFT));

  REQUIRE(viewport.hasHover());
  // The camera starts focused on the origin, so the centre of the screen is
  // inside tile (0, 0).
  REQUIRE(viewport.hoveredTile().x == Approx(0.0f));
  REQUIRE(viewport.hoveredTile().y == Approx(0.0f));
}

TEST_CASE("moving outside the viewport clears the hover") {
  EditorViewportWidget viewport = makeViewport();
  const float cx = viewport.rect.x + viewport.rect.w * 0.5f;
  const float cy = viewport.rect.y + viewport.rect.h * 0.5f;

  viewport.handleMouseMove(mouseAt(cx, cy, eng::GuiMouseButton::LEFT));
  REQUIRE(viewport.hasHover());

  viewport.handleMouseMove(mouseAt(-50.0f, -50.0f, eng::GuiMouseButton::LEFT));
  REQUIRE_FALSE(viewport.hasHover());
}

TEST_CASE("hovered tiles are floored, so a whole tile maps to one coordinate") {
  EditorViewportWidget viewport = makeViewport();
  const IsoView view = makeIsoView(viewport.camera, viewport.rect);

  // Two points well inside the same tile must report the same coordinate.
  const IsoPoint a = worldToScreen(view, {3.2f, 5.2f});
  const IsoPoint b = worldToScreen(view, {3.8f, 5.8f});

  viewport.handleMouseMove(mouseAt(a.x, a.y, eng::GuiMouseButton::LEFT));
  const WorldPoint first = viewport.hoveredTile();
  viewport.handleMouseMove(mouseAt(b.x, b.y, eng::GuiMouseButton::LEFT));
  const WorldPoint second = viewport.hoveredTile();

  REQUIRE(first.x == Approx(3.0f));
  REQUIRE(first.y == Approx(5.0f));
  REQUIRE(second.x == Approx(first.x));
  REQUIRE(second.y == Approx(first.y));
}

TEST_CASE("scroll always consumes the event so it never bubbles") {
  EditorViewportWidget viewport = makeViewport();
  eng::GuiScrollEvent scroll{};
  scroll.x = 400.0f;
  scroll.y = 300.0f;
  scroll.delta_y = 1.0f;

  REQUIRE(viewport.handleScroll(scroll));
  REQUIRE(viewport.camera.zoom > 1.0f);

  // Still consumed once the camera is pinned at maximum zoom.
  viewport.camera.zoom = ISO_ZOOM_MAX;
  REQUIRE(viewport.handleScroll(scroll));
  REQUIRE(viewport.camera.zoom == Approx(ISO_ZOOM_MAX));
}

TEST_CASE("clone preserves camera state") {
  EditorViewportWidget viewport = makeViewport();
  viewport.camera.zoom = 2.5f;
  viewport.camera.focus = {12.0f, -8.0f};

  auto copy = viewport.clone();
  auto* typed = dynamic_cast<EditorViewportWidget*>(copy.get());
  REQUIRE(typed != nullptr);
  REQUIRE(typed->camera.zoom == Approx(2.5f));
  REQUIRE(typed->camera.focus.x == Approx(12.0f));
}

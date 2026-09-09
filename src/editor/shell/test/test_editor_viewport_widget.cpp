#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-viewport-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>

using Catch::Approx;
using namespace eng::editor;

namespace {

EditorViewportWidget makeViewport() {
  EditorViewportWidget viewport;
  viewport.rect = eng::makeRect(0.0f, 64.0f, 800.0f, 600.0f);
  return viewport;
}

/// A marker occupying one tile, standing on the ground.
EditorPlacementMarker markerOnTile(float x, float y) {
  return {{{x, y, 0.0f}, {x + 1.0f, y + 1.0f, 1.0f}}, false};
}

/// The same marker, as the selected one.
EditorPlacementMarker selectedMarkerOnTile(float x, float y) {
  EditorPlacementMarker marker = markerOnTile(x, y);
  marker.selected = true;
  return marker;
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

/// Render the viewport through a device-less renderer and hand back the
/// command stream, which is where clipping actually lives.
struct RenderedViewport {
  eng::GuiRendererContext renderer;

  explicit RenderedViewport(const EditorViewportWidget& viewport) {
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = 1280;
    renderer.viewport_height = 800;
    renderer.beginFrame();
    eng::GuiDrawContext ctx;
    ctx.renderer = &renderer;
    viewport.render(ctx);
  }
  ~RenderedViewport() { renderer.shutdown(); }
  RenderedViewport(const RenderedViewport&) = delete;
  RenderedViewport& operator=(const RenderedViewport&) = delete;
  RenderedViewport(RenderedViewport&&) = delete;
  RenderedViewport& operator=(RenderedViewport&&) = delete;
};

}  // namespace

TEST_CASE("the viewport paints no opaque background over itself") {
  // 3D geometry is drawn in the scene pass, which runs before the GUI pass.
  // A filled quad covering the viewport rect would erase every mesh in it,
  // which is exactly what used to happen; the frame clear provides the
  // background instead.
  const EditorViewportWidget viewport = makeViewport();
  const RenderedViewport rendered(viewport);
  const auto& vertices = rendered.renderer.vertices;

  for (size_t q = 0; q + 3 < vertices.size(); q += 4) {
    // A border-only quad draws a frame, not a fill, so it may span the rect.
    if (vertices[q].border_width > 0.0f) {
      continue;
    }
    float min_x = vertices[q].pos[0];
    float max_x = vertices[q].pos[0];
    float min_y = vertices[q].pos[1];
    float max_y = vertices[q].pos[1];
    for (size_t i = 1; i < 4; ++i) {
      min_x = std::min(min_x, vertices[q + i].pos[0]);
      max_x = std::max(max_x, vertices[q + i].pos[0]);
      min_y = std::min(min_y, vertices[q + i].pos[1]);
      max_y = std::max(max_y, vertices[q + i].pos[1]);
    }
    const bool covers_viewport = min_x <= viewport.rect.x &&
                                 min_y <= viewport.rect.y &&
                                 max_x >= viewport.rect.x + viewport.rect.w &&
                                 max_y >= viewport.rect.y + viewport.rect.h;
    REQUIRE_FALSE(covers_viewport);
  }
}

TEST_CASE("the scene composites between the ground and the cursor") {
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(2.0f, 3.0f));
  viewport.handleMouseMove(mouseAt(400.0f, 300.0f, eng::GuiMouseButton::LEFT));
  const RenderedViewport rendered(viewport);
  const auto& renderer = rendered.renderer;

  // Grid, axes and placements draw under the 3D; the hover highlight draws
  // over it. A split at either end would put all of it on one side.
  const size_t split = renderer.sceneSplit();
  REQUIRE(split > 0);
  REQUIRE(split < renderer.commands.size());
}

TEST_CASE("the scissor is balanced on both sides of the scene split") {
  EditorViewportWidget viewport = makeViewport();
  viewport.handleMouseMove(mouseAt(400.0f, 300.0f, eng::GuiMouseButton::LEFT));
  const RenderedViewport rendered(viewport);
  const auto& commands = rendered.renderer.commands;
  const size_t split = rendered.renderer.sceneSplit();

  // Each half is submitted as its own render pass, and a clip cannot span
  // two passes: a push left open at the split would never be popped, and
  // its pop would arrive in a pass that never pushed.
  int depth = 0;
  for (size_t i = 0; i < commands.size(); ++i) {
    if (i == split) {
      REQUIRE(depth == 0);
    }
    if (commands[i].type == eng::DrawCommandType::PUSH_SCISSOR) {
      ++depth;
    }
    if (commands[i].type == eng::DrawCommandType::POP_SCISSOR) {
      --depth;
    }
    REQUIRE(depth >= 0);
  }
  REQUIRE(depth == 0);
}

TEST_CASE("the grid is clipped to the viewport") {
  const EditorViewportWidget viewport = makeViewport();
  const RenderedViewport rendered(viewport);

  // The grid runs well past the widget by design, so without a scissor
  // command in the stream it paints over the chrome above it.
  const auto& commands = rendered.renderer.commands;
  size_t pushes = 0;
  for (const auto& cmd : commands) {
    if (cmd.type != eng::DrawCommandType::PUSH_SCISSOR) {
      continue;
    }
    ++pushes;
    REQUIRE(cmd.scissor.x == Approx(viewport.rect.x));
    REQUIRE(cmd.scissor.y == Approx(viewport.rect.y));
    REQUIRE(cmd.scissor.w == Approx(viewport.rect.w));
    REQUIRE(cmd.scissor.h == Approx(viewport.rect.h));
  }
  REQUIRE(pushes == 1);
}

TEST_CASE("the grid lines are batched between the push and the pop") {
  const EditorViewportWidget viewport = makeViewport();
  const RenderedViewport rendered(viewport);
  const auto& commands = rendered.renderer.commands;

  // Ordering is what makes the clip effective. The background fill and the
  // border are the widget's own rect, so they sit outside the pair; every
  // line that can overrun has to sit inside it.
  size_t push = commands.size();
  size_t pop = commands.size();
  for (size_t i = 0; i < commands.size(); ++i) {
    if (commands[i].type == eng::DrawCommandType::PUSH_SCISSOR) {
      push = i;
    }
    if (commands[i].type == eng::DrawCommandType::POP_SCISSOR) {
      pop = i;
    }
  }
  REQUIRE(push < pop);
  REQUIRE(pop < commands.size());
  REQUIRE(pop - push > 1);
}

TEST_CASE("hiding the grid keeps the clip in place for the axes") {
  EditorViewportWidget viewport = makeViewport();
  viewport.show_grid = false;
  const RenderedViewport rendered(viewport);

  bool clipped = false;
  for (const auto& cmd : rendered.renderer.commands) {
    clipped = clipped || cmd.type == eng::DrawCommandType::PUSH_SCISSOR;
  }
  REQUIRE(clipped);
}

TEST_CASE("hiding the grid emits far less geometry") {
  EditorViewportWidget shown = makeViewport();
  EditorViewportWidget hidden = makeViewport();
  hidden.show_grid = false;

  const RenderedViewport with_grid(shown);
  const RenderedViewport without_grid(hidden);

  // Command count is unchanged — contiguous lines merge into one batch —
  // so the vertex buffer is what shows the grid actually went away.
  REQUIRE(without_grid.renderer.vertices.size() <
          with_grid.renderer.vertices.size());
}

TEST_CASE("a zero-area viewport draws nothing at all") {
  EditorViewportWidget viewport = makeViewport();
  viewport.rect = eng::makeRect(0.0f, 0.0f, 0.0f, 0.0f);
  const RenderedViewport rendered(viewport);

  REQUIRE(rendered.renderer.commands.empty());
}

TEST_CASE("a middle-drag starts a pan and moves the camera") {
  EditorViewportWidget viewport = makeViewport();
  REQUIRE(viewport.handleMouseDown(
      mouseAt(400.0f, 300.0f, eng::GuiMouseButton::MIDDLE)));

  viewport.handleMouseMove(
      mouseAt(440.0f, 320.0f, eng::GuiMouseButton::MIDDLE));

  REQUIRE(viewport.camera.focus.x == Approx(-40.0f));
  REQUIRE(viewport.camera.focus.y == Approx(-20.0f));
}

TEST_CASE("a plain left-drag pans the camera") {
  EditorViewportWidget viewport = makeViewport();
  REQUIRE(viewport.handleMouseDown(
      mouseAt(400.0f, 300.0f, eng::GuiMouseButton::LEFT)));

  viewport.handleMouseMove(mouseAt(360.0f, 280.0f, eng::GuiMouseButton::LEFT));

  // Dragging left pulls the world left, so the focus moves right.
  REQUIRE(viewport.camera.focus.x == Approx(40.0f));
  REQUIRE(viewport.camera.focus.y == Approx(20.0f));
}

TEST_CASE("shift plus left-drag still pans") {
  EditorViewportWidget viewport = makeViewport();
  REQUIRE(viewport.handleMouseDown(
      mouseAt(100.0f, 100.0f, eng::GuiMouseButton::LEFT, /*shift=*/true)));

  viewport.handleMouseMove(
      mouseAt(140.0f, 100.0f, eng::GuiMouseButton::LEFT, /*shift=*/true));

  REQUIRE(viewport.camera.focus.x == Approx(-40.0f));
}

TEST_CASE("a right-drag does not pan") {
  EditorViewportWidget viewport = makeViewport();
  // Right-click is left free for a context menu.
  REQUIRE_FALSE(viewport.handleMouseDown(
      mouseAt(100.0f, 100.0f, eng::GuiMouseButton::RIGHT)));

  viewport.handleMouseMove(mouseAt(300.0f, 300.0f, eng::GuiMouseButton::RIGHT));
  REQUIRE(viewport.camera.focus.x == Approx(0.0f));
  REQUIRE(viewport.camera.focus.y == Approx(0.0f));
}

TEST_CASE("a left-drag pans one-to-one with the cursor at any zoom") {
  for (float zoom : {0.5f, 1.0f, 2.0f}) {
    EditorViewportWidget viewport = makeViewport();
    viewport.camera.zoom = zoom;
    const IsoView view = makeIsoView(viewport.camera, viewport.rect);
    const IsoPoint before = worldToScreen(view, {3.0f, 4.0f});

    viewport.handleMouseDown(
        mouseAt(before.x, before.y, eng::GuiMouseButton::LEFT));
    viewport.handleMouseMove(
        mouseAt(before.x + 60.0f, before.y - 25.0f, eng::GuiMouseButton::LEFT));

    // The world point grabbed at mouse-down stays under the cursor.
    const IsoView after = makeIsoView(viewport.camera, viewport.rect);
    const IsoPoint now = worldToScreen(after, {3.0f, 4.0f});
    REQUIRE(now.x == Approx(before.x + 60.0f));
    REQUIRE(now.y == Approx(before.y - 25.0f));
  }
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

TEST_CASE("a left click on a placement picks it") {
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(0.0f, 0.0f));
  int picked = -2;
  viewport.on_placement_picked = [&picked](int index) {
    picked = index;
  };

  const IsoView view = makeIsoView(viewport.camera, viewport.rect);
  const IsoPoint on_it = worldToScreen(view, {0.5f, 0.5f, 0.5f});
  viewport.handleMouseDown(
      mouseAt(on_it.x, on_it.y, eng::GuiMouseButton::LEFT));
  viewport.handleMouseUp(mouseAt(on_it.x, on_it.y, eng::GuiMouseButton::LEFT));

  REQUIRE(picked == 0);
}

TEST_CASE("a left click on bare ground picks nothing") {
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(0.0f, 0.0f));
  int picked = -2;
  viewport.on_placement_picked = [&picked](int index) {
    picked = index;
  };

  const IsoView view = makeIsoView(viewport.camera, viewport.rect);
  const IsoPoint empty = worldToScreen(view, {5.5f, 5.5f, 0.0f});
  viewport.handleMouseDown(
      mouseAt(empty.x, empty.y, eng::GuiMouseButton::LEFT));
  viewport.handleMouseUp(mouseAt(empty.x, empty.y, eng::GuiMouseButton::LEFT));

  // Reported, not ignored: clicking empty ground is how a selection is
  // dropped.
  REQUIRE(picked == EDITOR_PLACEMENT_NONE);
}

TEST_CASE("a pan does not pick") {
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(0.0f, 0.0f));
  bool reported = false;
  viewport.on_placement_picked = [&reported](int) {
    reported = true;
  };

  const IsoView view = makeIsoView(viewport.camera, viewport.rect);
  const IsoPoint on_it = worldToScreen(view, {0.5f, 0.5f, 0.5f});
  viewport.handleMouseDown(
      mouseAt(on_it.x, on_it.y, eng::GuiMouseButton::LEFT));
  viewport.handleMouseMove(
      mouseAt(on_it.x + 60.0f, on_it.y, eng::GuiMouseButton::LEFT));
  viewport.handleMouseUp(
      mouseAt(on_it.x + 60.0f, on_it.y, eng::GuiMouseButton::LEFT));

  // Dragging to move the view must not change what is selected, or every
  // pan would land the panel on something new.
  REQUIRE_FALSE(reported);
}

TEST_CASE("a middle click never picks") {
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(0.0f, 0.0f));
  bool reported = false;
  viewport.on_placement_picked = [&reported](int) {
    reported = true;
  };

  const IsoView view = makeIsoView(viewport.camera, viewport.rect);
  const IsoPoint on_it = worldToScreen(view, {0.5f, 0.5f, 0.5f});
  viewport.handleMouseDown(
      mouseAt(on_it.x, on_it.y, eng::GuiMouseButton::MIDDLE));
  viewport.handleMouseUp(
      mouseAt(on_it.x, on_it.y, eng::GuiMouseButton::MIDDLE));

  REQUIRE_FALSE(reported);
}

TEST_CASE("a click a pixel or two off still counts as a click") {
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(0.0f, 0.0f));
  int picked = -2;
  viewport.on_placement_picked = [&picked](int index) {
    picked = index;
  };

  // A hand shifts on the way to letting go of a button; that is a click.
  const IsoView view = makeIsoView(viewport.camera, viewport.rect);
  const IsoPoint on_it = worldToScreen(view, {0.5f, 0.5f, 0.5f});
  viewport.handleMouseDown(
      mouseAt(on_it.x, on_it.y, eng::GuiMouseButton::LEFT));
  viewport.handleMouseMove(
      mouseAt(on_it.x + 2.0f, on_it.y + 1.0f, eng::GuiMouseButton::LEFT));
  viewport.handleMouseUp(
      mouseAt(on_it.x + 2.0f, on_it.y + 1.0f, eng::GuiMouseButton::LEFT));

  REQUIRE(picked == 0);
}

TEST_CASE("the selected placement is outlined as a box over the scene") {
  EditorViewportWidget plain = makeViewport();
  plain.placement_markers.push_back(markerOnTile(2.0f, 3.0f));
  EditorViewportWidget selected = makeViewport();
  selected.placement_markers.push_back(selectedMarkerOnTile(2.0f, 3.0f));

  const RenderedViewport without(plain);
  const RenderedViewport with(selected);

  // Twelve more edges, drawn after the scene split so the box reads against
  // the mesh it belongs to rather than behind it.
  REQUIRE(with.renderer.vertices.size() > without.renderer.vertices.size());
  REQUIRE(with.renderer.sceneSplit() < with.renderer.commands.size() - 1);
}

TEST_CASE("nothing selected and no hover leaves the overlay clip unopened") {
  // An empty push/pop pair around nothing is work the renderer does not
  // need to be given.
  EditorViewportWidget viewport = makeViewport();
  viewport.placement_markers.push_back(markerOnTile(2.0f, 3.0f));
  const RenderedViewport rendered(viewport);

  size_t pushes = 0;
  for (const auto& cmd : rendered.renderer.commands) {
    pushes += cmd.type == eng::DrawCommandType::PUSH_SCISSOR ? 1 : 0;
  }
  REQUIRE(pushes == 1);
}

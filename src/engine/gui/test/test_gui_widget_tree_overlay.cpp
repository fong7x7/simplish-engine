#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-widget-tree.h>

using namespace eng;

namespace {

/// A laid-out root with a button in it.
struct OverlayFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId button{tree.createWidget(GuiWidgetType::BUTTON, root)};

  OverlayFixture() {
    tree.findWidget(button)->tree_layout.height = 30.0f;
    tree.computeLayout({0, 0, 400, 300});
  }
};

}  // namespace

TEST_CASE("the overlay layer covers the root, above everything, see-through") {
  OverlayFixture fx;
  const GuiWidgetId layer = fx.tree.overlayLayer();
  REQUIRE(layer != GUI_WIDGET_ID_INVALID);
  CHECK(fx.tree.overlayLayer() == layer);
  const GuiWidget& w = *fx.tree.findWidget(layer);
  CHECK(w.rect.w == 400.0f);
  CHECK(w.pointer_through);
  CHECK(w.z_index == GUI_OVERLAY_LAYER_Z);
  // The pointer goes through it to the button.
  CHECK(fx.tree.hitTest(10, 10).widget_id == fx.button);
}

TEST_CASE("adding or removing a widget asks for a layout") {
  OverlayFixture fx;
  CHECK_FALSE(fx.tree.needsLayout());
  const GuiWidgetId extra = fx.tree.createWidget(GuiWidgetType::PANEL, fx.root);
  CHECK(fx.tree.needsLayout());
  fx.tree.computeLayout({0, 0, 400, 300});
  fx.tree.destroyWidget(extra);
  CHECK(fx.tree.needsLayout());
}

TEST_CASE("a tooltip shows after the pointer rests, and a press hides it") {
  OverlayFixture fx;
  fx.tree.findWidget(fx.button)->tooltip = "Save the level";
  GuiRendererContext renderer;
  GuiDrawContext ctx;
  ctx.renderer = &renderer;
  auto quads = [&] {
    renderer.beginFrame();
    fx.tree.renderAll(ctx);
    return renderer.vertices.size();
  };
  fx.tree.updateHover(10, 10);
  const size_t without = quads();
  fx.tree.updateAll(ctx, GUI_TOOLTIP_DELAY_SECONDS + 0.01f);
  CHECK(quads() > without);
  fx.tree.dispatchMouseDown({.x = 10, .y = 10});
  CHECK(quads() == without);
}

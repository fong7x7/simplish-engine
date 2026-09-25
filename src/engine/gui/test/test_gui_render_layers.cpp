#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-widget-tree.h>

using namespace eng;

namespace {

/// A panel containing a panel, rendered through a real renderer.
struct LayerFixture {
  GuiWidgetTree tree;
  GuiRendererContext renderer;
  GuiWidgetId outer{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId inner{tree.createWidget(GuiWidgetType::PANEL, outer)};

  LayerFixture() {
    panel(outer).rect = {0, 0, 100, 100};
    panel(outer).fill_color = {255, 255, 255, 255};
    panel(inner).rect = {40, 40, 20, 20};
    panel(inner).fill_color = {255, 0, 0, 255};
  }

  GuiPanel& panel(GuiWidgetId id) {
    return *dynamic_cast<GuiPanel*>(tree.findWidget(id));
  }

  void render() {
    renderer.beginFrame();
    GuiDrawContext ctx;
    ctx.renderer = &renderer;
    tree.renderAll(ctx);
  }

  /// The inner panel's first vertex.
  const GuiVertex& innerQuad() const { return renderer.vertices[4]; }
};

}  // namespace

TEST_CASE("a widget's opacity fades its children with it") {
  LayerFixture fx;
  fx.panel(fx.outer).opacity = 0.5f;
  fx.render();
  CHECK((fx.renderer.vertices[0].color >> 24U) == 127U);
  CHECK((fx.innerQuad().color >> 24U) == 127U);
}

TEST_CASE("a widget's render scale draws its subtree scaled about its centre") {
  LayerFixture fx;
  fx.panel(fx.outer).render_scale = 0.5f;
  fx.render();
  // The outer box shrinks about (50, 50); the child goes with it.
  CHECK(fx.renderer.vertices[0].pos[0] == 25.0f);
  CHECK(fx.innerQuad().pos[0] == 45.0f);
  CHECK(fx.innerQuad().rect_w == 10.0f);
  // Layout is untouched.
  CHECK(fx.panel(fx.inner).rect.x == 40.0f);
}

TEST_CASE("a render offset moves the subtree, and the frame after resets") {
  LayerFixture fx;
  fx.panel(fx.inner).render_offset_y = 7.0f;
  fx.render();
  CHECK(fx.innerQuad().pos[1] == 47.0f);
  CHECK(fx.renderer.transform.offset_y == 0.0f);
  CHECK(fx.renderer.alpha_scale == 1.0f);
}

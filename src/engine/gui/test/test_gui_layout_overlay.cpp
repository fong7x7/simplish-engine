#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-widget-tree.h>

using namespace eng;

namespace {

/// A panel padded 10 holding a child with a margin of 5, laid out in a
/// 200 × 100 view.
struct Inspected {
  GuiWidgetTree tree;
  GuiRendererContext renderer;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId child{tree.createWidget(GuiWidgetType::PANEL, root)};

  Inspected() {
    tree.findWidget(root)->tree_layout.padding = {10, 10, 10, 10};
    tree.findWidget(child)->tree_layout.margin = {5, 5, 5, 5};
    tree.findWidget(child)->tree_layout.height = 30.0f;
    tree.computeLayout({0, 0, 200, 100});
  }

  /// How many quads a frame draws.
  size_t quads() {
    renderer.beginFrame();
    GuiDrawContext ctx;
    ctx.renderer = &renderer;
    tree.renderAll(ctx);
    return renderer.vertices.size() / 4;
  }

  /// Whether a quad was drawn exactly over @p box.
  [[nodiscard]] bool drewOver(const Rect& box) const {
    return std::ranges::any_of(renderer.vertices, [&box](const GuiVertex& v) {
      return v.pos[0] == box.x && v.pos[1] == box.y;
    });
  }
};

}  // namespace

TEST_CASE("the layout overlay is off until asked for, then outlines boxes") {
  Inspected fx;
  const size_t plain = fx.quads();
  fx.tree.layout_overlay = GuiLayoutOverlay::BOXES;
  CHECK(fx.quads() >= plain + 2);  // a hairline round each widget
}

TEST_CASE("the layout overlay inspects the deepest widget under the pointer") {
  Inspected fx;
  fx.tree.layout_overlay = GuiLayoutOverlay::BOXES;
  CHECK(fx.tree.inspectedWidget() == nullptr);

  fx.tree.findWidget(fx.root)->hovered = true;
  fx.tree.findWidget(fx.child)->hovered = true;
  REQUIRE(fx.tree.inspectedWidget() == fx.tree.findWidget(fx.child));

  (void)fx.quads();
  // The margin ring starts 5 outside the child's box, at the root's
  // padding edge.
  const Rect& box = fx.tree.findWidget(fx.child)->rect;
  CHECK(box.x == 15.0f);
  CHECK(fx.drewOver({10.0f, 10.0f, 0.0f, 0.0f}));
}

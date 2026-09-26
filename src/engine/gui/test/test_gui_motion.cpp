#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-scroll-panel.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>

using namespace eng;

namespace {

/// A column of three 20-pixel rows that glide, in a 100 × 100 view.
struct Rows {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId rows[3]{};

  Rows() {
    for (GuiWidgetId& row : rows) {
      row = tree.createWidget(GuiWidgetType::PANEL, root);
      tree.findWidget(row)->tree_layout.height = 20.0f;
      tree.findWidget(row)->layout_glide = 0.2f;
    }
    layout();
  }
  void layout() { tree.computeLayout({0, 0, 100, 100}); }
  GuiWidget& row(size_t i) { return *tree.findWidget(rows[i]); }
};

}  // namespace

TEST_CASE("a widget moved by layout glides from its old place") {
  Rows r;
  CHECK(r.row(2).render_offset_y == 0.0f);  // the first layout: no glide

  r.row(0).visible = false;  // the rows below close up
  r.tree.markDirty(r.root);
  r.layout();

  CHECK(r.row(2).rect.y == 20.0f);
  CHECK(r.row(2).render_offset_y == 20.0f);  // drawn where it was
  r.tree.updateAll({}, 0.1f);
  CHECK(r.row(2).render_offset_y > 0.0f);
  CHECK(r.row(2).render_offset_y < 20.0f);
  r.tree.updateAll({}, 0.2f);
  CHECK(r.row(2).render_offset_y == 0.0f);
}

TEST_CASE("a widget without a glide jumps; one with reduced motion lands") {
  Rows r;
  r.row(2).layout_glide = 0.0f;
  r.row(0).visible = false;
  r.tree.markDirty(r.root);
  r.layout();
  CHECK(r.row(2).render_offset_y == 0.0f);

  GuiDrawContext still;
  still.motion = GuiMotion::REDUCED;
  r.tree.updateAll(still, 0.001f);
  CHECK(r.row(1).render_offset_y == 0.0f);
}

TEST_CASE("scrolling does not glide what it scrolls") {
  GuiWidgetTree tree;
  const GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  auto& list = dynamic_cast<GuiScrollPanel&>(*tree.findWidget(
      tree.insertExternalWidget(std::make_unique<GuiScrollPanel>(), root)));
  list.tree_layout.height = 50.0f;
  const GuiWidgetId row =
      tree.createWidget(GuiWidgetType::PANEL, list.widget_id);
  for (int i = 0; i < 5; ++i) {
    tree.createWidget(GuiWidgetType::PANEL, list.widget_id);
  }
  tree.findWidget(row)->layout_glide = 0.2f;
  tree.computeLayout({0, 0, 100, 100});
  const float before = tree.findWidget(row)->rect.y;

  REQUIRE(list.scrollBy(0.0f, 30.0f));
  tree.markDirty(root);
  tree.computeLayout({0, 0, 100, 100});

  CHECK(tree.findWidget(row)->rect.y < before);  // it did scroll
  CHECK(tree.findWidget(row)->render_offset_y == 0.0f);
}

TEST_CASE("a dismissed widget leaves, then is destroyed after the frame") {
  GuiWidgetTree tree;
  const GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  const GuiWidgetId sheet = tree.createWidget(GuiWidgetType::PANEL, root);

  tree.dismiss(sheet, GUI_PRESENCE_RISE.exiting());
  CHECK(tree.findWidget(sheet)->pointer_through);
  tree.updateAll({}, 0.05f);
  REQUIRE(tree.findWidget(sheet) != nullptr);
  CHECK(tree.findWidget(sheet)->opacity < 1.0f);
  CHECK(tree.findWidget(sheet)->render_offset_y > 0.0f);

  tree.updateAll({}, 1.0f);
  CHECK(tree.findWidget(sheet) == nullptr);
}

TEST_CASE("an entering widget starts where its presence says, and rests") {
  GuiWidgetTree tree;
  const GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  GuiWidget& menu = *tree.findWidget(root);

  menu.enter(GUI_PRESENCE_DROP);
  CHECK(menu.opacity == 0.0f);
  CHECK(menu.render_offset_y == GUI_PRESENCE_DROP.offset_y);
  tree.updateAll({}, 1.0f);
  CHECK(menu.opacity == 1.0f);
  CHECK(menu.render_offset_y == 0.0f);
}

#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-widget-tree.h>

using namespace eng;

namespace {

constexpr Rect WINDOW{0.0f, 0.0f, 1000.0f, 800.0f};

/// A tree with a window-sized root, the arrangement every screen needs.
struct TreeFixture {
  GuiWidgetTree tree;
  GuiWidgetId root = GUI_WIDGET_ID_INVALID;

  TreeFixture() {
    root = tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
    tree.findWidget(root)->rect = WINDOW;
  }

  /// Add a child of `parent` occupying `rect`.
  GuiWidgetId add(GuiWidgetId parent, const Rect& rect) {
    GuiWidgetId id = tree.createWidget(GuiWidgetType::PANEL, parent);
    tree.findWidget(id)->rect = rect;
    return id;
  }
};

}  // namespace

TEST_CASE("the first widget attached with no parent becomes the root") {
  GuiWidgetTree tree;
  REQUIRE(tree.root_id == GUI_WIDGET_ID_INVALID);

  const GuiWidgetId first =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);

  // Passing `tree.root_id` as the parent before a root exists therefore
  // makes that widget the root, rather than adding a child to one.
  REQUIRE(tree.root_id == first);
}

TEST_CASE("hit testing is bounded by the root rect") {
  TreeFixture fx;
  // A root that does not cover the window is the whole trap: hit testing
  // starts at the root and stops dead when the cursor is outside it, so
  // every widget below it becomes unclickable no matter where it sits.
  fx.tree.findWidget(fx.root)->rect = {0.0f, 0.0f, 1000.0f, 28.0f};
  const GuiWidgetId below = fx.add(fx.root, {0.0f, 90.0f, 1000.0f, 600.0f});

  REQUIRE(fx.tree.hitTest(500.0f, 300.0f).widget_id == GUI_WIDGET_ID_INVALID);
  REQUIRE(fx.tree.findWidget(below) != nullptr);
}

TEST_CASE("a widget under a window-sized root is hit") {
  TreeFixture fx;
  const GuiWidgetId viewport = fx.add(fx.root, {0.0f, 90.0f, 1000.0f, 600.0f});

  REQUIRE(fx.tree.hitTest(500.0f, 300.0f).widget_id == viewport);
}

TEST_CASE("a child outside its parent's rect is never hit") {
  TreeFixture fx;
  const GuiWidgetId bar = fx.add(fx.root, {0.0f, 0.0f, 1000.0f, 26.0f});
  // A dropdown hanging below its bar, if it were parented to it.
  const GuiWidgetId menu = fx.add(bar, {0.0f, 26.0f, 200.0f, 240.0f});

  REQUIRE(fx.tree.hitTest(100.0f, 100.0f).widget_id != menu);
}

TEST_CASE("a sibling with a higher z_index wins the hit") {
  TreeFixture fx;
  const GuiWidgetId under = fx.add(fx.root, {0.0f, 0.0f, 500.0f, 500.0f});
  const GuiWidgetId over = fx.add(fx.root, {0.0f, 0.0f, 500.0f, 500.0f});
  fx.tree.findWidget(over)->z_index = 10;

  REQUIRE(fx.tree.hitTest(100.0f, 100.0f).widget_id == over);
  REQUIRE(fx.tree.findWidget(under) != nullptr);
}

TEST_CASE("z_index beats insertion order in both directions") {
  TreeFixture fx;
  const GuiWidgetId first = fx.add(fx.root, {0.0f, 0.0f, 500.0f, 500.0f});
  fx.add(fx.root, {0.0f, 0.0f, 500.0f, 500.0f});
  fx.tree.findWidget(first)->z_index = 10;

  // Inserted first, but on top: later siblings do not automatically win.
  REQUIRE(fx.tree.hitTest(100.0f, 100.0f).widget_id == first);
}

TEST_CASE("an invisible widget is skipped, and so are its children") {
  TreeFixture fx;
  const GuiWidgetId under = fx.add(fx.root, {0.0f, 0.0f, 500.0f, 500.0f});
  const GuiWidgetId scrim = fx.add(fx.root, {0.0f, 0.0f, 1000.0f, 800.0f});
  fx.tree.findWidget(scrim)->z_index = 10;
  const GuiWidgetId child = fx.add(scrim, {0.0f, 0.0f, 100.0f, 100.0f});
  fx.tree.findWidget(scrim)->visible = false;

  REQUIRE(fx.tree.hitTest(50.0f, 50.0f).widget_id == under);
  REQUIRE(fx.tree.findWidget(child) != nullptr);
}

TEST_CASE("a child is hit in preference to its parent") {
  TreeFixture fx;
  const GuiWidgetId panel = fx.add(fx.root, {0.0f, 0.0f, 500.0f, 500.0f});
  const GuiWidgetId button = fx.add(panel, {10.0f, 10.0f, 80.0f, 24.0f});

  REQUIRE(fx.tree.hitTest(20.0f, 20.0f).widget_id == button);
  REQUIRE(fx.tree.hitTest(300.0f, 300.0f).widget_id == panel);
}

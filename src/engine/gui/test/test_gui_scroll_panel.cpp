#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-scroll-panel.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <vector>

using namespace eng;

namespace {

/// A 100-pixel-tall list of ten 30-pixel buttons, 10 apart: 390 pixels of
/// content, so it scrolls 290.
struct ListFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiScrollPanel* list = nullptr;
  std::vector<GuiWidgetId> rows;

  ListFixture() {
    tree.findWidget(root)->rect = {0.0f, 0.0f, 1000.0f, 800.0f};
    auto panel = std::make_unique<GuiScrollPanel>();
    panel->tree_layout.gap = 10.0f;
    list = panel.get();
    const GuiWidgetId id = tree.insertExternalWidget(std::move(panel), root);
    for (int i = 0; i < 10; ++i) {
      rows.push_back(tree.createWidget(GuiWidgetType::BUTTON, id));
      tree.findWidget(rows.back())->tree_layout.height = 30.0f;
    }
    tree.arrangeWidget(id, {0.0f, 0.0f, 200.0f, 100.0f});
  }

  /// Where row @p i is now.
  [[nodiscard]] const Rect& row(size_t i) const {
    return tree.findWidget(rows[i])->rect;
  }
};

}  // namespace

TEST_CASE("a scroll panel stacks its children at their own heights") {
  ListFixture fx;
  REQUIRE(fx.row(0).y == 0.0f);
  REQUIRE(fx.row(0).h == 30.0f);
  REQUIRE(fx.row(1).y == 40.0f);
  REQUIRE(fx.row(9).y == 360.0f);
  REQUIRE(fx.row(3).w == 200.0f);
  REQUIRE(fx.list->maxScroll() == 290.0f);
}

TEST_CASE("the wheel scrolls the panel and moves its children") {
  ListFixture fx;
  // Over a button: the button does not scroll, so the list under it does.
  REQUIRE(fx.tree.dispatchScroll({.x = 50.0f, .y = 10.0f, .delta_y = -1.0f}));
  REQUIRE(fx.list->scrollOffset() == 40.0f);
  REQUIRE(fx.row(1).y == 0.0f);

  // It stops at the top.
  REQUIRE(fx.tree.dispatchScroll({.x = 50.0f, .y = 10.0f, .delta_y = 1.0f}));
  REQUIRE_FALSE(
      fx.tree.dispatchScroll({.x = 50.0f, .y = 10.0f, .delta_y = 1.0f}));
  REQUIRE(fx.list->scrollOffset() == 0.0f);
}

TEST_CASE("moving focus down the list scrolls just enough to show it") {
  ListFixture fx;
  fx.tree.setFocus(fx.rows[1]);
  REQUIRE(fx.list->scrollOffset() == 0.0f);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == fx.rows[2]);
  // Row 2 spans 80-110; the view ends at 100, so 10 up.
  REQUIRE(fx.list->scrollOffset() == 10.0f);
  REQUIRE(fx.row(2).y + fx.row(2).h == 100.0f);

  fx.tree.setFocus(fx.rows[0]);
  REQUIRE(fx.list->scrollOffset() == 0.0f);
}

TEST_CASE("focusing the last row scrolls to the bottom") {
  ListFixture fx;
  fx.tree.setFocus(fx.rows[9]);
  REQUIRE(fx.list->scrollOffset() == fx.list->maxScroll());
}

TEST_CASE("with nothing focusable below, down scrolls the list instead") {
  // One Accept button over 400 pixels of terms in a 100-pixel view.
  ListFixture fx;
  for (size_t i = 1; i < fx.rows.size(); ++i) {
    fx.tree.findWidget(fx.rows[i])->tree_focusable = false;
  }
  fx.tree.setFocus(fx.rows[0]);

  REQUIRE(fx.tree.routeNav(GuiNavCommand::DOWN));
  REQUIRE(fx.tree.focused_id == fx.rows[0]);
  REQUIRE(fx.list->scrollOffset() == fx.list->nav_step);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::UP));
  REQUIRE(fx.list->scrollOffset() == 0.0f);
  REQUIRE_FALSE(fx.tree.routeNav(GuiNavCommand::UP));
}

TEST_CASE("a list that fits does not scroll") {
  ListFixture fx;
  fx.tree.arrangeWidget(fx.list->widget_id, {0.0f, 0.0f, 200.0f, 500.0f});
  REQUIRE(fx.list->maxScroll() == 0.0f);
  REQUIRE_FALSE(
      fx.tree.dispatchScroll({.x = 50.0f, .y = 10.0f, .delta_y = -1.0f}));
}

TEST_CASE("a scroll panel clips its children to inside its padding") {
  ListFixture fx;
  fx.list->tree_layout.padding = {.top = 5, .right = 6, .bottom = 7, .left = 8};
  fx.tree.arrangeWidget(fx.list->widget_id, {0.0f, 0.0f, 200.0f, 100.0f});
  REQUIRE(fx.list->childClipRect().has_value());
  const Rect clip = fx.list->childClipRect().value_or(Rect{});
  REQUIRE(clip.x == 8.0f);
  REQUIRE(clip.y == 5.0f);
  REQUIRE(clip.w == 186.0f);
  REQUIRE(clip.h == 88.0f);
  REQUIRE(fx.row(0).x == 8.0f);
}

TEST_CASE("hidden children take no room in the column") {
  ListFixture fx;
  fx.tree.findWidget(fx.rows[0])->visible = false;
  fx.tree.arrangeWidget(fx.list->widget_id, {0.0f, 0.0f, 200.0f, 100.0f});
  REQUIRE(fx.row(1).y == 0.0f);
}

namespace {

/// A 100-pixel-wide row of five 60-pixel cards, 10 apart: 340 pixels of
/// content, so it scrolls 240.
struct RowFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiScrollPanel* strip = nullptr;
  std::vector<GuiWidgetId> cards;

  RowFixture() {
    tree.findWidget(root)->rect = {0.0f, 0.0f, 1000.0f, 800.0f};
    auto panel = std::make_unique<GuiScrollPanel>();
    panel->axis = GuiScrollAxis::HORIZONTAL;
    panel->item_size = 60.0f;
    panel->tree_layout.gap = 10.0f;
    strip = panel.get();
    const GuiWidgetId id = tree.insertExternalWidget(std::move(panel), root);
    for (int i = 0; i < 5; ++i) {
      cards.push_back(tree.createWidget(GuiWidgetType::BUTTON, id));
    }
    tree.arrangeWidget(id, {0.0f, 0.0f, 100.0f, 80.0f});
  }
};

}  // namespace

TEST_CASE("a horizontal panel lays its children out in a row") {
  RowFixture fx;
  const Rect& second = fx.tree.findWidget(fx.cards[1])->rect;
  REQUIRE(second.x == 70.0f);
  REQUIRE(second.w == 60.0f);
  REQUIRE(second.h == 80.0f);
  REQUIRE(fx.strip->maxScroll() == 240.0f);
}

TEST_CASE("a horizontal panel scrolls sideways to what is focused") {
  RowFixture fx;
  fx.tree.setFocus(fx.cards[0]);
  REQUIRE(fx.tree.routeNav(GuiNavCommand::RIGHT));
  REQUIRE(fx.tree.focused_id == fx.cards[1]);
  // Card 1 spans 70-130; the view ends at 100, so 30 along.
  REQUIRE(fx.strip->scrollOffset() == 30.0f);
}

TEST_CASE("the wheel scrolls a horizontal panel sideways") {
  RowFixture fx;
  REQUIRE(fx.tree.dispatchScroll({.x = 20.0f, .y = 20.0f, .delta_y = -1.0f}));
  REQUIRE(fx.strip->scrollOffset() == fx.strip->wheel_step);
}

TEST_CASE("scrolling by pixels moves only along the panel's axis") {
  RowFixture fx;
  fx.tree.setFocus(fx.cards[0]);
  REQUIRE_FALSE(fx.tree.scrollFocusBy(0.0f, 50.0f));
  REQUIRE(fx.tree.scrollFocusBy(25.0f, 0.0f));
  REQUIRE(fx.strip->scrollOffset() == 25.0f);
  REQUIRE(fx.tree.findWidget(fx.cards[0])->rect.x == -25.0f);
}

TEST_CASE("the right stick scrolls the list focus is in") {
  ListFixture fx;
  fx.tree.setFocus(fx.rows[0]);
  REQUIRE(fx.tree.scrollFocusBy(0.0f, 60.0f));
  REQUIRE(fx.list->scrollOffset() == 60.0f);
  // Focus stays where it was; the stick reads, it does not move focus.
  REQUIRE(fx.tree.focused_id == fx.rows[0]);
}

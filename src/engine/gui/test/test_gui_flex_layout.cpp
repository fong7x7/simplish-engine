#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <utility>

using namespace eng;
using Catch::Approx;

namespace {

/// A tree whose root panel is laid out in a 400 × 300 viewport.
struct FlexFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};

  /// The root's layout style.
  LayoutStyle& rootStyle() { return tree.findWidget(root)->tree_layout; }

  /// A panel under @p parent, @p w × @p h (-1 for auto).
  GuiWidgetId add(GuiWidgetId parent, float w, float h) {
    const GuiWidgetId id = tree.createWidget(GuiWidgetType::PANEL, parent);
    style(id).width = w;
    style(id).height = h;
    return id;
  }

  LayoutStyle& style(GuiWidgetId id) {
    return tree.findWidget(id)->tree_layout;
  }

  [[nodiscard]] const Rect& rect(GuiWidgetId id) {
    return tree.findWidget(id)->rect;
  }

  void layout() { tree.computeLayout({0.0f, 0.0f, 400.0f, 300.0f}); }
};

/// Whether @p r is where @p want is, and as big.
bool at(const Rect& r, const Rect& want) {
  return r.x == Approx(want.x) && r.y == Approx(want.y) &&
         r.w == Approx(want.w) && r.h == Approx(want.h);
}

/// The x of two 100-wide children in a 400-wide row justified by @p align.
std::pair<float, float> justified(Align align) {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().justify_content = align;
  const GuiWidgetId a = fx.add(fx.root, 100.0f, -1.0f);
  const GuiWidgetId b = fx.add(fx.root, 100.0f, -1.0f);
  fx.layout();
  return {fx.rect(a).x, fx.rect(b).x};
}

}  // namespace

TEST_CASE("a column stacks its children inside its padding, a gap apart") {
  FlexFixture fx;
  fx.rootStyle().padding = {10.0f, 20.0f, 10.0f, 20.0f};
  fx.rootStyle().gap = 5.0f;
  const GuiWidgetId a = fx.add(fx.root, -1.0f, 30.0f);
  const GuiWidgetId b = fx.add(fx.root, -1.0f, 40.0f);
  fx.layout();
  // Stretched across the content box: 400 less 20 each side.
  REQUIRE(at(fx.rect(a), {20.0f, 10.0f, 360.0f, 30.0f}));
  REQUIRE(at(fx.rect(b), {20.0f, 45.0f, 360.0f, 40.0f}));
}

TEST_CASE("margins push a child off its siblings and its parent's edge") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().align_items = Align::START;
  fx.rootStyle().gap = 4.0f;
  const GuiWidgetId a = fx.add(fx.root, 50.0f, 20.0f);
  const GuiWidgetId b = fx.add(fx.root, 50.0f, 20.0f);
  fx.style(a).margin = {3.0f, 6.0f, 0.0f, 8.0f};
  fx.style(b).margin = {0.0f, 0.0f, 0.0f, 10.0f};
  fx.layout();
  REQUIRE(at(fx.rect(a), {8.0f, 3.0f, 50.0f, 20.0f}));
  // 8 + 50 + 6 of a, the gap, then b's own 10.
  REQUIRE(at(fx.rect(b), {78.0f, 0.0f, 50.0f, 20.0f}));
}

TEST_CASE("a stretched child loses its cross margins") {
  FlexFixture fx;
  const GuiWidgetId a = fx.add(fx.root, -1.0f, 20.0f);
  fx.style(a).margin = {0.0f, 15.0f, 0.0f, 5.0f};
  fx.layout();
  REQUIRE(at(fx.rect(a), {5.0f, 0.0f, 380.0f, 20.0f}));
}

TEST_CASE("grow factors share the space left over in proportion") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId fixed = fx.add(fx.root, 100.0f, -1.0f);
  const GuiWidgetId one = fx.add(fx.root, -1.0f, -1.0f);
  const GuiWidgetId three = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(one).flex_grow = 1.0f;
  fx.style(three).flex_grow = 3.0f;
  fx.layout();
  REQUIRE(fx.rect(fixed).w == Approx(100.0f));
  REQUIRE(at(fx.rect(one), {100.0f, 0.0f, 75.0f, 300.0f}));
  REQUIRE(at(fx.rect(three), {175.0f, 0.0f, 225.0f, 300.0f}));
}

TEST_CASE("an explicit size is the basis a child grows from") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId a = fx.add(fx.root, 100.0f, -1.0f);
  const GuiWidgetId b = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(a).flex_grow = 1.0f;
  fx.style(b).flex_grow = 1.0f;
  fx.style(b).flex_basis = 0.0f;
  fx.layout();
  REQUIRE(fx.rect(a).w == Approx(250.0f));
  REQUIRE(fx.rect(b).w == Approx(150.0f));
}

TEST_CASE("a max holds a growing child back and its share goes to the rest") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId capped = fx.add(fx.root, -1.0f, -1.0f);
  const GuiWidgetId flexible = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(capped).flex_grow = 1.0f;
  fx.style(capped).max_width = 50.0f;
  fx.style(flexible).flex_grow = 1.0f;
  fx.layout();
  REQUIRE(fx.rect(capped).w == Approx(50.0f));
  REQUIRE(at(fx.rect(flexible), {50.0f, 0.0f, 350.0f, 300.0f}));
}

TEST_CASE("overflowing children shrink by shrink factor times size") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId a = fx.add(fx.root, 300.0f, -1.0f);
  const GuiWidgetId b = fx.add(fx.root, 200.0f, -1.0f);
  const GuiWidgetId rigid = fx.add(fx.root, 50.0f, -1.0f);
  fx.style(rigid).flex_shrink = 0.0f;
  fx.layout();
  // 150 too much, taken 3:2 from a and b.
  REQUIRE(fx.rect(a).w == Approx(210.0f));
  REQUIRE(fx.rect(b).w == Approx(140.0f));
  REQUIRE(at(fx.rect(rigid), {350.0f, 0.0f, 50.0f, 300.0f}));
}

TEST_CASE("a min stops a child shrinking and the rest shrink more") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId a = fx.add(fx.root, 300.0f, -1.0f);
  const GuiWidgetId b = fx.add(fx.root, 300.0f, -1.0f);
  fx.style(a).min_width = 280.0f;
  fx.layout();
  REQUIRE(fx.rect(a).w == Approx(280.0f));
  REQUIRE(fx.rect(b).w == Approx(120.0f));
}

TEST_CASE("justify_content places the free space along the main axis") {
  // Two 100-wide children in 400: 200 free.
  CHECK(justified(Align::START) == std::pair{0.0f, 100.0f});
  CHECK(justified(Align::END) == std::pair{200.0f, 300.0f});
  CHECK(justified(Align::CENTER) == std::pair{100.0f, 200.0f});
  CHECK(justified(Align::SPACE_BETWEEN) == std::pair{0.0f, 300.0f});
  CHECK(justified(Align::SPACE_AROUND) == std::pair{50.0f, 250.0f});
  const auto evenly = justified(Align::SPACE_EVENLY);
  CHECK(evenly.first == Approx(200.0f / 3.0f));
  CHECK(evenly.second == Approx(100.0f + 400.0f / 3.0f));
}

TEST_CASE("align_items and align_self place children across the line") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().align_items = Align::CENTER;
  const GuiWidgetId centred = fx.add(fx.root, 10.0f, 100.0f);
  const GuiWidgetId end = fx.add(fx.root, 10.0f, 100.0f);
  const GuiWidgetId stretched = fx.add(fx.root, 10.0f, -1.0f);
  const GuiWidgetId fixed = fx.add(fx.root, 10.0f, 40.0f);
  fx.style(end).align_self = Align::END;
  fx.style(stretched).align_self = Align::STRETCH;
  fx.style(stretched).max_height = 250.0f;
  fx.style(fixed).align_self = Align::STRETCH;
  fx.layout();
  REQUIRE(fx.rect(centred).y == Approx(100.0f));
  REQUIRE(fx.rect(end).y == Approx(200.0f));
  REQUIRE(fx.rect(stretched).h == Approx(250.0f));
  // An explicit height is kept even when asked to stretch.
  REQUIRE(at(fx.rect(fixed), {30.0f, 0.0f, 10.0f, 40.0f}));
}

TEST_CASE("a wrapping row breaks into lines as tall as their tallest") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().wrap = FlexWrap::WRAP;
  fx.rootStyle().align_items = Align::START;
  fx.rootStyle().gap = 10.0f;
  const GuiWidgetId a = fx.add(fx.root, 150.0f, 30.0f);
  const GuiWidgetId b = fx.add(fx.root, 150.0f, 50.0f);
  const GuiWidgetId c = fx.add(fx.root, 150.0f, 20.0f);
  fx.layout();
  REQUIRE(at(fx.rect(a), {0.0f, 0.0f, 150.0f, 30.0f}));
  REQUIRE(at(fx.rect(b), {160.0f, 0.0f, 150.0f, 50.0f}));
  // 150 + 10 + 150 + 10 + 150 is over 400: c starts the second line.
  REQUIRE(at(fx.rect(c), {0.0f, 60.0f, 150.0f, 20.0f}));
}

TEST_CASE("align_content spreads wrapped lines across the container") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().wrap = FlexWrap::WRAP;
  fx.rootStyle().align_content = Align::END;
  const GuiWidgetId a = fx.add(fx.root, 300.0f, 100.0f);
  const GuiWidgetId b = fx.add(fx.root, 300.0f, 100.0f);
  fx.layout();
  REQUIRE(fx.rect(a).y == Approx(100.0f));
  REQUIRE(fx.rect(b).y == Approx(200.0f));
}

TEST_CASE("an absolute child sits at its offset, out of the flow") {
  FlexFixture fx;
  fx.rootStyle().padding = {10.0f, 10.0f, 10.0f, 10.0f};
  const GuiWidgetId badge = fx.add(fx.root, 20.0f, 20.0f);
  fx.style(badge).position = PositionMode::ABSOLUTE;
  fx.style(badge).abs_x = 370.0f;
  fx.style(badge).abs_y = 5.0f;
  fx.style(badge).margin.left = 2.0f;
  const GuiWidgetId row = fx.add(fx.root, -1.0f, 30.0f);
  fx.layout();
  REQUIRE(at(fx.rect(badge), {372.0f, 5.0f, 20.0f, 20.0f}));
  REQUIRE(at(fx.rect(row), {10.0f, 10.0f, 380.0f, 30.0f}));
}

TEST_CASE("a hidden child takes no space") {
  FlexFixture fx;
  const GuiWidgetId hidden = fx.add(fx.root, -1.0f, 50.0f);
  const GuiWidgetId shown = fx.add(fx.root, -1.0f, 50.0f);
  fx.tree.findWidget(hidden)->visible = false;
  fx.layout();
  REQUIRE(fx.rect(shown).y == Approx(0.0f));
}

TEST_CASE("a container measures to its children, gaps and padding") {
  FlexFixture fx;
  fx.rootStyle().align_items = Align::START;
  const GuiWidgetId row = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(row).direction = FlexDirection::ROW;
  fx.style(row).padding = {4.0f, 6.0f, 4.0f, 6.0f};
  fx.style(row).gap = 8.0f;
  const GuiWidgetId a = fx.add(row, 40.0f, 20.0f);
  const GuiWidgetId b = fx.add(row, 60.0f, 30.0f);
  fx.style(b).margin = {0.0f, 2.0f, 0.0f, 0.0f};
  const GuiWidgetId after = fx.add(fx.root, 10.0f, 10.0f);
  fx.layout();
  // 6 + 40 + 8 + 60 + 2 + 6 wide; 4 + 30 + 4 high.
  REQUIRE(at(fx.rect(row), {0.0f, 0.0f, 122.0f, 38.0f}));
  REQUIRE(at(fx.rect(a), {6.0f, 4.0f, 40.0f, 20.0f}));
  REQUIRE(fx.rect(after).y == Approx(38.0f));
}

TEST_CASE("min and max bound a container's measured size") {
  FlexFixture fx;
  fx.rootStyle().align_items = Align::START;
  const GuiWidgetId box = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(box).min_width = 90.0f;
  fx.style(box).max_height = 25.0f;
  fx.add(box, 50.0f, 60.0f);
  fx.layout();
  REQUIRE(fx.rect(box).w == Approx(90.0f));
  REQUIRE(fx.rect(box).h == Approx(25.0f));
}

TEST_CASE("a label measures to its text, and padding adds round it") {
  FlexFixture fx;
  fx.rootStyle().align_items = Align::START;
  auto label = std::make_unique<GuiLabel>();
  label->text = "Hello";
  label->tree_layout.padding = {2.0f, 5.0f, 2.0f, 5.0f};
  const GuiWidgetId id =
      fx.tree.insertExternalWidget(std::move(label), fx.root);
  fx.layout();
  const GuiDrawContext no_font{};
  REQUIRE(fx.rect(id).w == Approx(no_font.measureText("Hello") + 10.0f));
  REQUIRE(fx.rect(id).h == Approx(no_font.textLineHeight() + 4.0f));
}

TEST_CASE("nested containers lay out all the way down") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId sidebar = fx.add(fx.root, 100.0f, -1.0f);
  const GuiWidgetId main = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(main).flex_grow = 1.0f;
  fx.style(main).padding = {10.0f, 10.0f, 10.0f, 10.0f};
  const GuiWidgetId header = fx.add(main, -1.0f, 40.0f);
  const GuiWidgetId body = fx.add(main, -1.0f, -1.0f);
  fx.style(body).flex_grow = 1.0f;
  fx.layout();
  REQUIRE(at(fx.rect(sidebar), {0.0f, 0.0f, 100.0f, 300.0f}));
  REQUIRE(at(fx.rect(header), {110.0f, 10.0f, 280.0f, 40.0f}));
  REQUIRE(at(fx.rect(body), {110.0f, 50.0f, 280.0f, 240.0f}));
}

TEST_CASE("absolute insets anchor a child to the far edges or stretch it") {
  FlexFixture fx;
  const GuiWidgetId fill = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(fill).position = PositionMode::ABSOLUTE;
  fx.style(fill).abs_right = 0.0f;
  fx.style(fill).abs_bottom = 0.0f;
  const GuiWidgetId corner = fx.add(fx.root, 30.0f, 20.0f);
  fx.style(corner).position = PositionMode::ABSOLUTE;
  fx.style(corner).abs_right = 10.0f;
  fx.style(corner).abs_bottom = 5.0f;
  fx.style(corner).margin.right = 2.0f;
  fx.layout();
  REQUIRE(at(fx.rect(fill), {0.0f, 0.0f, 400.0f, 300.0f}));
  REQUIRE(at(fx.rect(corner), {358.0f, 275.0f, 30.0f, 20.0f}));
}

TEST_CASE("the layout leaves a manually placed child where it was put") {
  FlexFixture fx;
  const GuiWidgetId manual = fx.add(fx.root, -1.0f, 50.0f);
  fx.style(manual).position = PositionMode::MANUAL;
  fx.tree.findWidget(manual)->rect = {7.0f, 8.0f, 9.0f, 10.0f};
  const GuiWidgetId inner = fx.add(manual, 5.0f, 5.0f);
  fx.tree.findWidget(inner)->rect = {1.0f, 2.0f, 3.0f, 4.0f};
  const GuiWidgetId flowed = fx.add(fx.root, -1.0f, 50.0f);
  fx.layout();
  REQUIRE(at(fx.rect(manual), {7.0f, 8.0f, 9.0f, 10.0f}));
  REQUIRE(at(fx.rect(inner), {1.0f, 2.0f, 3.0f, 4.0f}));
  REQUIRE(fx.rect(flowed).y == Approx(0.0f));
}

TEST_CASE("an auto left margin pushes a child to the end of its row") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId a = fx.add(fx.root, 50.0f, 20.0f);
  const GuiWidgetId b = fx.add(fx.root, 50.0f, 20.0f);
  fx.style(b).margin_auto.left = true;
  fx.layout();
  CHECK(fx.rect(a).x == Approx(0.0f));
  CHECK(fx.rect(b).x == Approx(350.0f));
}

TEST_CASE("auto margins on both sides centre, and share the room evenly") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId a = fx.add(fx.root, 100.0f, 20.0f);
  fx.style(a).margin_auto.left = true;
  fx.style(a).margin_auto.right = true;
  fx.layout();
  CHECK(fx.rect(a).x == Approx(150.0f));
}

TEST_CASE("auto margins across a line align a child in it") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  const GuiWidgetId bottom = fx.add(fx.root, 50.0f, 40.0f);
  const GuiWidgetId middle = fx.add(fx.root, 50.0f, -1.0f);
  fx.style(bottom).margin_auto.top = true;
  fx.style(middle).margin_auto.top = true;
  fx.style(middle).margin_auto.bottom = true;
  fx.style(middle).min_height = 20.0f;
  fx.layout();
  CHECK(fx.rect(bottom).y == Approx(260.0f));
  // An auto margin across stops the stretch.
  CHECK(fx.rect(middle).h == Approx(20.0f));
  CHECK(fx.rect(middle).y == Approx(140.0f));
}

TEST_CASE("percentage sizes are shares of the parent's content box") {
  FlexFixture fx;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().padding = {0.0f, 50.0f, 0.0f, 50.0f};
  fx.rootStyle().align_items = Align::START;
  const GuiWidgetId quarter = fx.add(fx.root, -1.0f, -1.0f);
  fx.style(quarter).width_percent = 25.0f;
  fx.style(quarter).height_percent = 50.0f;
  fx.layout();
  CHECK(fx.rect(quarter).w == Approx(75.0f));
  CHECK(fx.rect(quarter).h == Approx(150.0f));
}

TEST_CASE("an absolute child's percentages are shares of its parent's box") {
  FlexFixture fx;
  const GuiWidgetId badge = fx.add(fx.root, -1.0f, 20.0f);
  fx.style(badge).position = PositionMode::ABSOLUTE;
  fx.style(badge).width_percent = 10.0f;
  fx.style(badge).abs_right = 0.0f;
  fx.layout();
  CHECK(fx.rect(badge).w == Approx(40.0f));
  CHECK(fx.rect(badge).x == Approx(360.0f));
}

TEST_CASE("pixel snapping rounds placed edges to the step") {
  FlexFixture fx;
  fx.tree.pixel_snap = 1.0f;
  fx.rootStyle().direction = FlexDirection::ROW;
  fx.rootStyle().justify_content = Align::SPACE_EVENLY;
  const GuiWidgetId a = fx.add(fx.root, 100.0f, 20.0f);
  fx.add(fx.root, 100.0f, 20.0f);
  fx.layout();
  CHECK(fx.rect(a).x == 67.0f);  // 66.67 rounded
  CHECK(fx.rect(a).w == 100.0f);
}

TEST_CASE("updateLayout leaves clean subtrees where they are") {
  FlexFixture fx;
  const GuiWidgetId panel = fx.add(fx.root, -1.0f, 100.0f);
  const GuiWidgetId inner = fx.add(panel, -1.0f, 30.0f);
  const GuiDrawContext ctx{};
  fx.tree.updateLayout({0, 0, 400, 300}, ctx);
  // Moved by hand: a clean tree in the same viewport is not laid out again.
  fx.tree.findWidget(inner)->rect.x = 5.0f;
  fx.tree.updateLayout({0, 0, 400, 300}, ctx);
  CHECK(fx.rect(inner).x == 5.0f);
  // Marked dirty, it is.
  fx.tree.markDirty(inner);
  fx.tree.updateLayout({0, 0, 400, 300}, ctx);
  CHECK(fx.rect(inner).x == 0.0f);
  // So is everything when the viewport changes.
  fx.tree.updateLayout({0, 0, 500, 300}, ctx);
  CHECK(fx.rect(inner).w == 500.0f);
}

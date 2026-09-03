#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-asset-panel-widget.h>
#include <string>
#include <vector>

using Catch::Approx;
using namespace eng::editor;

namespace {

constexpr eng::Rect PANEL_RECT{0.0f, 684.0f, 1280.0f, ASSET_PANEL_HEIGHT};

EditorAssetPanelWidget makePanel() {
  EditorAssetPanelWidget panel;
  panel.rect = PANEL_RECT;
  panel.setAssetNames({"crate", "barrel", "wall"});
  return panel;
}

eng::GuiMouseEvent mouseAt(float x, float y) {
  eng::GuiMouseEvent event{};
  event.x = x;
  event.y = y;
  event.button = eng::GuiMouseButton::LEFT;
  return event;
}

/// Centre of the card at `index`.
eng::GuiMouseEvent onCard(const EditorAssetPanelWidget& panel, size_t index) {
  const eng::Rect card = panel.cardRect(index);
  return mouseAt(card.x + card.w * 0.5f, card.y + card.h * 0.5f);
}

}  // namespace

TEST_CASE("the panel lists one card per asset") {
  const EditorAssetPanelWidget panel = makePanel();
  REQUIRE(panel.assetCount() == 3);
}

TEST_CASE("cards are laid out left to right inside the panel") {
  const EditorAssetPanelWidget panel = makePanel();
  const eng::Rect first = panel.cardRect(0);
  const eng::Rect second = panel.cardRect(1);

  REQUIRE(second.x > first.x);
  REQUIRE(first.y >= PANEL_RECT.y);
  REQUIRE(first.y + first.h <= PANEL_RECT.y + PANEL_RECT.h);
}

TEST_CASE("hit testing finds the card that was drawn there") {
  EditorAssetPanelWidget panel = makePanel();
  for (size_t i = 0; i < panel.assetCount(); ++i) {
    const eng::Rect card = panel.cardRect(i);
    REQUIRE(panel.hitTestCard(card.x + 2.0f, card.y + 2.0f) ==
            static_cast<int>(i));
  }
}

TEST_CASE("a point in the panel but off a card hits nothing") {
  EditorAssetPanelWidget panel = makePanel();
  // The header strip above the cards.
  REQUIRE(panel.hitTestCard(PANEL_RECT.x + 4.0f, PANEL_RECT.y + 4.0f) == -1);
  // Past the last card.
  REQUIRE(panel.hitTestCard(PANEL_RECT.x + PANEL_RECT.w - 4.0f,
                            PANEL_RECT.y + 60.0f) == -1);
}

TEST_CASE("pressing a card starts a drag and captures the mouse") {
  EditorAssetPanelWidget panel = makePanel();
  // Capture is what lets the drag continue over the viewport.
  REQUIRE(panel.handleMouseDown(onCard(panel, 1)));
  REQUIRE(panel.draggingIndex() == 1);
}

TEST_CASE("pressing empty panel space starts nothing") {
  EditorAssetPanelWidget panel = makePanel();
  REQUIRE_FALSE(
      panel.handleMouseDown(mouseAt(PANEL_RECT.x + 4.0f, PANEL_RECT.y + 4.0f)));
  REQUIRE(panel.draggingIndex() == -1);
}

TEST_CASE("releasing outside the panel reports a drop") {
  EditorAssetPanelWidget panel = makePanel();
  size_t dropped = 99;
  float drop_x = 0.0f;
  float drop_y = 0.0f;
  panel.on_asset_dropped = [&](size_t index, float x, float y) {
    dropped = index;
    drop_x = x;
    drop_y = y;
  };

  panel.handleMouseDown(onCard(panel, 2));
  panel.handleMouseMove(mouseAt(600.0f, 300.0f));
  panel.handleMouseUp(mouseAt(600.0f, 300.0f));

  REQUIRE(dropped == 2);
  REQUIRE(drop_x == Approx(600.0f));
  REQUIRE(drop_y == Approx(300.0f));
  REQUIRE(panel.draggingIndex() == -1);
}

TEST_CASE("releasing back over the panel cancels the drag") {
  EditorAssetPanelWidget panel = makePanel();
  bool dropped = false;
  panel.on_asset_dropped = [&](size_t, float, float) {
    dropped = true;
  };

  panel.handleMouseDown(onCard(panel, 0));
  panel.handleMouseUp(onCard(panel, 0));

  // A mis-drag should cost nothing.
  REQUIRE_FALSE(dropped);
  REQUIRE(panel.draggingIndex() == -1);
}

TEST_CASE("a release with no drag in flight reports nothing") {
  EditorAssetPanelWidget panel = makePanel();
  bool dropped = false;
  panel.on_asset_dropped = [&](size_t, float, float) {
    dropped = true;
  };

  panel.handleMouseUp(mouseAt(600.0f, 300.0f));
  REQUIRE_FALSE(dropped);
}

TEST_CASE("replacing the asset list cancels any drag") {
  EditorAssetPanelWidget panel = makePanel();
  panel.handleMouseDown(onCard(panel, 0));
  // Opening another project mid-drag must not leave a dangling index.
  panel.setAssetNames({"other"});

  REQUIRE(panel.draggingIndex() == -1);
  REQUIRE(panel.assetCount() == 1);
}

TEST_CASE("an empty panel hit-tests to nothing") {
  EditorAssetPanelWidget panel;
  panel.rect = PANEL_RECT;
  REQUIRE(panel.hitTestCard(PANEL_RECT.x + 20.0f, PANEL_RECT.y + 60.0f) == -1);
  REQUIRE_FALSE(panel.handleMouseDown(mouseAt(20.0f, 700.0f)));
}

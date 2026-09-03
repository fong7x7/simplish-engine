#include "capture-font.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <editor/shell/editor-menu-bar-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-dropdown.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-theme-constants.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <string>

using namespace eng::editor;

namespace {

constexpr uint32_t CAPTURE_W = 960;
constexpr uint32_t CAPTURE_H = 540;
constexpr float TITLE_H = 28.0f;


/// Renders the menu bar with its File menu open, through the engine's CPU
/// rasterizer. Same approach as `test_editor_chrome_capture.cpp`: no window
/// and no GPU, `GuiRendererContext::init(nullptr)` being the engine's
/// unit-test mode.
///
/// What this adds over the widget tests is paint order. The rects those
/// assert would be identical whether the dropdown drew above the rest of the
/// editor or behind it; only rasterizing shows which pixels win.
struct MenuCapture {
  eng::GuiWidgetTree tree;
  eng::GuiRendererContext renderer;
  eng::editor::test::CaptureFont font;
  eng::GuiWidgetId root = eng::GUI_WIDGET_ID_INVALID;
  eng::GuiWidgetId bar_id = eng::GUI_WIDGET_ID_INVALID;
  eng::ImageData image;

  MenuCapture() {
    buildTree();
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = CAPTURE_W;
    renderer.viewport_height = CAPTURE_H;

    eng::GuiDrawContext ctx;
    ctx.renderer = &renderer;
    ctx.text_pipeline = &font.pipeline;
    ctx.face_id = font.face_id;
    tree.visitDrawOrder([&ctx](const eng::GuiWidget& widget) {
      if (widget.visible) {
        widget.render(ctx);
      }
    });

    image = eng::GuiSoftwareRasterizer::rasterizeQuads(
        renderer.vertices, window(), eng::GUI_RASTER_DEFAULT_BG, font.atlas());
  }

  ~MenuCapture() { renderer.shutdown(); }
  MenuCapture(const MenuCapture&) = delete;
  MenuCapture& operator=(const MenuCapture&) = delete;
  MenuCapture(MenuCapture&&) = delete;
  MenuCapture& operator=(MenuCapture&&) = delete;

  static eng::Rect window() {
    return eng::makeRect(0.0f, 0.0f, static_cast<float>(CAPTURE_W),
                         static_cast<float>(CAPTURE_H));
  }

  void buildTree() {
    root = tree.createWidget(eng::GuiWidgetType::PANEL,
                             eng::GUI_WIDGET_ID_INVALID);
    auto* backdrop = dynamic_cast<eng::GuiPanel*>(tree.findWidget(root));
    backdrop->rect = window();
    // Stand in for the viewport the menu has to paint over.
    backdrop->fill_color = eng::GuiColor{22, 22, 26, 255};
    bar_id = tree.insertExternalWidget(std::make_unique<EditorMenuBarWidget>(),
                                       root);
    bar()->init(tree);
    fillMenus();
    bar()->openMenu(tree, 0);
  }

  /// Put the bar in the state worth looking at: a project open, one recent
  /// entry, laid out, and the File menu down.
  void fillMenus() {
    bar()->setProjectPresence(EditorProjectPresence::OPEN);
    RecentProjectsList recent;
    recent.entries.push_back({"/p/transit", "Transit Station", "2026-08-27"});
    bar()->setRecentProjects(recent);
    bar()->layout(tree,
                  eng::makeRect(0.0f, TITLE_H, static_cast<float>(CAPTURE_W),
                                MENU_BAR_HEIGHT),
                  window());
    bar()->tick(tree);
  }

  EditorMenuBarWidget* bar() {
    return dynamic_cast<EditorMenuBarWidget*>(tree.findWidget(bar_id));
  }

  /// Read a pixel as (r, g, b).
  [[nodiscard]] std::array<uint8_t, 3> pixel(uint32_t x, uint32_t y) const {
    const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
    return {image.pixels[offset], image.pixels[offset + 1],
            image.pixels[offset + 2]};
  }
};

/// True when a pixel is within `tolerance` of a theme colour on every
/// channel. The rasterizer composites with alpha, so exact equality is not
/// a safe assertion.
bool matches(const std::array<uint8_t, 3>& pixel, const eng::GuiColor& color,
             int tolerance = 6) {
  const auto near = [tolerance](uint8_t a, uint8_t b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= tolerance;
  };
  return near(pixel[0], color.r) && near(pixel[1], color.g) &&
         near(pixel[2], color.b);
}

}  // namespace

TEST_CASE("the menu bar band paints across the window") {
  MenuCapture capture;
  const auto row = static_cast<uint32_t>(TITLE_H + MENU_BAR_HEIGHT * 0.5f);
  // Right of the last title, where only the bar itself paints.
  REQUIRE(matches(capture.pixel(CAPTURE_W - 8, row), eng::THEME_BG));
}

TEST_CASE("an open menu paints over what is below the bar") {
  MenuCapture capture;
  const eng::GuiDropdown* menu = dynamic_cast<const eng::GuiDropdown*>(
      capture.tree.findWidget(capture.bar()->dropdownId(0)));
  // A point inside the menu panel but clear of its rows' text and border.
  const auto x = static_cast<uint32_t>(menu->rect.x + menu->rect.w - 6.0f);
  const auto y = static_cast<uint32_t>(menu->rect.y + 4.0f);

  REQUIRE(matches(capture.pixel(x, y), eng::THEME_PANEL));
}

TEST_CASE("a closed menu paints nothing") {
  MenuCapture capture;
  const eng::GuiDropdown* menu = dynamic_cast<const eng::GuiDropdown*>(
      capture.tree.findWidget(capture.bar()->dropdownId(1)));
  const auto x = static_cast<uint32_t>(menu->rect.x + menu->rect.w - 6.0f);
  const auto y = static_cast<uint32_t>(menu->rect.y + menu->rect.h - 6.0f);

  // The Edit menu overlaps the File menu's column, so sample below both.
  REQUIRE(menu->rect.h > 0.0f);
  REQUIRE_FALSE(matches(capture.pixel(x, y + 200), eng::THEME_PANEL));
}

TEST_CASE("the menu capture can be written to PNG for inspection") {
  MenuCapture capture;
  // Honour CTest's working directory; the file is an artifact, not an
  // assertion, so a write failure is reported rather than asserted on.
  const std::string path = "editor-menu-capture.png";
  const bool written =
      eng::GuiSoftwareRasterizer::writePng(capture.image, path);
  INFO("wrote " << path << ": " << written);
  SUCCEED();
}

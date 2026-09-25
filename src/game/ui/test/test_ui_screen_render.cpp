#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/gui/gui-software-rasterizer.h>
#include <game/ui/ui-screen-json.h>
#include <game/ui/ui-screen-render.h>

using namespace eng;
using namespace eng::game;

namespace {

/// A centred menu: a panel filled bright red, holding one button.
constexpr std::string_view MENU = R"json({
  "root": {"type": "panel", "width": 200, "padding": 20,
           "fill": "#ff0000ff",
    "children": [{"type": "button", "text": "Play", "action": "play",
                  "fill": "#0000ffff", "height": 40}]}})json";

/// The pixel at @p x, @p y of @p image, as `0xAABBGGRR`.
uint32_t pixel(const ImageData& image, uint32_t x, uint32_t y) {
  const size_t at = (static_cast<size_t>(y) * image.width + x) * 4;
  return static_cast<uint32_t>(image.pixels[at]) |
         (static_cast<uint32_t>(image.pixels[at + 1]) << 8U) |
         (static_cast<uint32_t>(image.pixels[at + 2]) << 16U) |
         (static_cast<uint32_t>(image.pixels[at + 3]) << 24U);
}

}  // namespace

TEST_CASE("a screen renders to an image, its buttons where they were laid "
          "out") {
  const UiScreen menu = *parseUiScreen(MENU, "menu").screen;

  const UiScreenRender render = renderUiScreen(menu, {}, {400, 300});
  (void)GuiSoftwareRasterizer::writePng(render.image, "ui-screen-capture.png");

  REQUIRE(render.image.width == 400);
  REQUIRE(render.buttons.size() == 1);
  const Rect play = render.buttons[0].rect;
  CHECK((play.x == 120.0F && play.w == 160.0F));
  // Blue inside the button's top edge, red in the panel to its left.
  const auto y_mid = static_cast<uint32_t>(play.y + play.h / 2.0F);
  const auto x_mid = static_cast<uint32_t>(play.x + play.w / 2.0F);
  CHECK((pixel(render.image, x_mid, static_cast<uint32_t>(play.y) + 2) &
         0xFFU) < 0x40U);
  CHECK((pixel(render.image, static_cast<uint32_t>(play.x) - 10, y_mid) &
         0xFFU) > 0xC0U);
}

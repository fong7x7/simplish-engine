#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-font-discovery.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/text-pipeline.h>
#include <memory>

using namespace eng;
using Catch::Approx;

namespace {

/// The machine's UI font at every family weight, in a CPU pipeline, and a
/// draw context over it. `ready` is false on a machine with no font.
struct TextFixture {
  TextPipelineContext pipeline;
  GuiRendererContext renderer;
  GuiDrawContext ctx;
  bool ready = false;

  TextFixture() {
    const auto font = pipeline.init() ? selectGuiUiFont({}) : std::nullopt;
    const auto face =
        font ? pipeline.loadFontFamily(font->file_path) : std::nullopt;
    ready = face.has_value();
    if (ready) {
      // Glyph quads need a texture handle; the CPU rasterizer samples
      // atlas pixels, so any will do.
      pipeline.atlases.at(0).texture = 1;
      ctx.text_pipeline = &pipeline;
      ctx.face_id = *face;
    }
    ctx.renderer = &renderer;
    renderer.beginFrame();
  }
  ~TextFixture() { pipeline.shutdown(); }
  TextFixture(const TextFixture&) = delete;
  TextFixture& operator=(const TextFixture&) = delete;
  TextFixture(TextFixture&&) = delete;
  TextFixture& operator=(TextFixture&&) = delete;
};

constexpr GuiFont BODY{};
constexpr GuiFont BOLD{.weight = 700};
constexpr GuiFont BIG{.size = 28.0f};

}  // namespace

TEST_CASE("text grows with its size and its weight") {
  TextFixture fx;
  if (!fx.ready) {
    SKIP("no UI font on this machine");
  }
  const float body = fx.ctx.measureText("Settings", BODY);
  // Twice the size, about twice as wide: hinting rounds each advance, so
  // it is not exact.
  const float big = fx.ctx.measureText("Settings", BIG);
  CHECK(big > body * 1.8f);
  CHECK(big < body * 2.2f);
  CHECK(fx.ctx.measureText("Settings", BOLD) > body);
  CHECK(fx.ctx.fontMetrics(BIG).line_height >
        fx.ctx.fontMetrics(BODY).line_height * 1.8f);
  // Each weight is its own face.
  CHECK(fx.ctx.faceFor(BOLD) != fx.ctx.faceFor(BODY));
}

TEST_CASE("the old calls are the default font") {
  TextFixture fx;
  if (!fx.ready) {
    SKIP("no UI font on this machine");
  }
  CHECK(fx.ctx.measureText("Hello") == fx.ctx.measureText("Hello", BODY));
  CHECK(fx.ctx.textLineHeight() == fx.ctx.fontMetrics(BODY).line_height);
}

TEST_CASE("UTF-8 is read a character at a time") {
  const GuiDrawContext no_font{};
  // Without a font each character is one placeholder box: "é" is two
  // bytes but one character.
  CHECK(no_font.measureText("\xC3\xA9") == no_font.measureText("e"));
  CHECK(no_font.measureText("caf\xC3\xA9") == no_font.measureText("cafe"));
}

TEST_CASE("letter spacing adds between characters") {
  TextFixture fx;
  if (!fx.ready) {
    SKIP("no UI font on this machine");
  }
  const GuiFont spaced{.letter_spacing = 2.0f};
  CHECK(fx.ctx.measureText("ABCD", spaced) ==
        Approx(fx.ctx.measureText("ABCD", BODY) + 6.0f));
}

TEST_CASE("tabular digits are all one width") {
  TextFixture fx;
  if (!fx.ready) {
    SKIP("no UI font on this machine");
  }
  const GuiFont tabular{.digits = GuiDigits::TABULAR};
  CHECK(fx.ctx.measureText("111", tabular) ==
        Approx(fx.ctx.measureText("888", tabular)));
}

TEST_CASE("wrapping breaks at spaces to fit, and at newlines") {
  const GuiDrawContext no_font{};  // 8 pixels a character
  const auto lines = no_font.wrapText("one two three", BODY, 60.0f);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].begin == 0);
  CHECK(lines[0].end == 7);  // "one two"
  CHECK(lines[1].begin == 8);
  CHECK(lines[0].width == Approx(56.0f));
  CHECK(no_font.wrapText("a\nb", BODY, 1000.0f).size() == 2);
  // A word wider than the line breaks inside it.
  CHECK(no_font.wrapText("abcdefghij", BODY, 40.0f).size() == 2);
}

TEST_CASE("an ellipsis cuts a line to its width") {
  const GuiDrawContext no_font{};
  CHECK(no_font.ellipsize("short", BODY, 100.0f) == "short");
  const std::string cut = no_font.ellipsize("a rather long title", BODY, 64.0f);
  // Seven 8-pixel characters and an 8-pixel "…" fill 64.
  CHECK(cut == "a rathe\xE2\x80\xA6");
  CHECK(no_font.measureText(cut, BODY) <= 64.0f);
}

TEST_CASE("a wrapping label measures as tall as its lines at its width") {
  GuiWidgetTree tree;
  const GuiWidgetId root =
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID);
  tree.findWidget(root)->tree_layout.width = 60.0f;
  auto label = std::make_unique<GuiLabel>();
  label->text = "one two three four";
  label->wrap = GuiTextWrap::WORD;
  const GuiWidgetId id = tree.insertExternalWidget(std::move(label), root);
  tree.computeLayout({0.0f, 0.0f, 60.0f, 400.0f});
  // Without a font lines are 14 high: "one two" / "three" / "four".
  CHECK(tree.findWidget(id)->rect.h == Approx(42.0f));
}

namespace {

/// A line in every role, one under another, into @p fx's renderer; the
/// height they take.
float drawEveryRole(TextFixture& fx, const GuiTheme& t) {
  float y = 10.0f;
  for (const GuiTextRole role :
       {GuiTextRole::DISPLAY, GuiTextRole::TITLE, GuiTextRole::HEADING,
        GuiTextRole::BODY, GuiTextRole::LABEL, GuiTextRole::CAPTION}) {
    const GuiFont& font = t.font(role);
    fx.ctx.drawText({.text = "Wave 7 \xE2\x80\x94 Survive the horde",
                     .pos = {12.0f, y},
                     .color = t.palette.text,
                     .font = font});
    y += fx.ctx.fontMetrics(font).line_height + 6.0f;
  }
  return y + 10.0f;
}

}  // namespace

TEST_CASE("text in every role, captured for a look") {
  TextFixture fx;
  if (!fx.ready) {
    SKIP("no UI font on this machine");
  }
  const GuiTheme& t = GuiTheme::dark();
  const float height = drawEveryRole(fx, t);
  const auto& page = fx.pipeline.atlases.at(0);
  const ImageData image = GuiSoftwareRasterizer::rasterizeQuads(
      fx.renderer.vertices, {0.0f, 0.0f, 520.0f, height},
      t.palette.background.pack(), {page.rgba_pixels, page.width, page.height});
  CHECK(GuiSoftwareRasterizer::writePng(image, "gui-text-roles-capture.png"));
}

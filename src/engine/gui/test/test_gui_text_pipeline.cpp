#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-font-discovery.h>
#include <engine/gui/text-pipeline.h>
#include <optional>

using namespace eng;

namespace {

/// A text pipeline with the machine's UI font loaded, or no face when the
/// machine has none. No GPU device: glyphs still rasterize into the CPU
/// atlas, only the atlas texture handle stays invalid.
struct LoadedFont {
  TextPipelineContext pipeline;
  std::optional<uint32_t> face;

  LoadedFont() {
    REQUIRE(pipeline.init());
    auto chosen = selectGuiUiFont({});
    if (!chosen.has_value()) {
      return;
    }
    face = pipeline.loadFont(chosen->file_path, 400, FontLoadItalic::NORMAL);
  }

  ~LoadedFont() { pipeline.shutdown(); }
  LoadedFont(const LoadedFont&) = delete;
  LoadedFont& operator=(const LoadedFont&) = delete;
  LoadedFont(LoadedFont&&) = delete;
  LoadedFont& operator=(LoadedFont&&) = delete;
};

}  // namespace

TEST_CASE("face ids start at 1, so zero can never name a loaded face") {
  LoadedFont font;
  if (!font.face.has_value()) {
    SKIP("no usable font on this machine");
  }
  // `RenderedGameClient::guiTextFaceId` used to return a literal 0, which no
  // face could ever match, so every drawText fell back to placeholder boxes.
  REQUIRE(*font.face >= 1);
}

TEST_CASE("a loaded face rasterizes a real glyph") {
  LoadedFont font;
  if (!font.face.has_value()) {
    SKIP("no usable font on this machine");
  }
  const GlyphInfo* glyph = font.pipeline.ensureGlyph(*font.face, 'A');

  REQUIRE(glyph != nullptr);
  REQUIRE(glyph->advance > 0.0f);
  REQUIRE(glyph->atlas_w > 0);
  REQUIRE(glyph->atlas_h > 0);
}

TEST_CASE("rasterizing a glyph writes coverage into the atlas") {
  LoadedFont font;
  if (!font.face.has_value()) {
    SKIP("no usable font on this machine");
  }
  REQUIRE(font.pipeline.ensureGlyph(*font.face, 'A') != nullptr);

  // The atlas starts zeroed; a glyph that drew nothing would leave it that
  // way, which is what an unloaded or bitmap-only face looks like.
  bool any_coverage = false;
  for (uint8_t byte : font.pipeline.atlases.at(0).rgba_pixels) {
    any_coverage = any_coverage || byte != 0;
  }
  REQUIRE(any_coverage);
}

TEST_CASE("an unknown face id yields no glyph") {
  LoadedFont font;
  REQUIRE(font.pipeline.ensureGlyph(0, 'A') == nullptr);
  REQUIRE(font.pipeline.ensureGlyph(9999, 'A') == nullptr);
}

TEST_CASE("glyph metrics follow the requested raster height") {
  LoadedFont font;
  if (!font.face.has_value()) {
    SKIP("no usable font on this machine");
  }
  const float small = font.pipeline.ensureGlyph(*font.face, 'A')->advance;

  font.pipeline.setFontRasterHeight(*font.face, 32);
  const float large = font.pipeline.ensureGlyph(*font.face, 'A')->advance;

  REQUIRE(large > small);
}

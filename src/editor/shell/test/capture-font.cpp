#include "capture-font.h"

#include <engine/gui/gui-font-discovery.h>

namespace eng::editor::test {

namespace {

  /// Regular weight in the CSS-style scale `loadFont` takes.
  constexpr uint16_t REGULAR_WEIGHT = 400;

  /// Stand-in for a GPU atlas texture. `emitGlyph` drops glyphs whose atlas
  /// has no texture handle, and there is no device here to make one; the CPU
  /// rasterizer samples atlas pixels rather than textures, so any non-zero
  /// handle is enough to get the glyph quads emitted.
  constexpr eng::RhiTextureHandle STAND_IN_TEXTURE = 1;

}  // namespace

CaptureFont::CaptureFont() {
  if (!pipeline.init()) {
    return;
  }
  auto chosen = eng::selectGuiUiFont({});
  if (!chosen.has_value()) {
    return;
  }
  auto loaded = pipeline.loadFont(chosen->file_path, REGULAR_WEIGHT,
                                  eng::FontLoadItalic::NORMAL);
  if (!loaded.has_value()) {
    return;
  }
  face_id = *loaded;
  pipeline.atlases.at(0).texture = STAND_IN_TEXTURE;
}

}  // namespace eng::editor::test

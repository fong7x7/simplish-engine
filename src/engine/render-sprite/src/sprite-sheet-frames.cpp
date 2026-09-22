#include <algorithm>
#include <cmath>
#include <engine/render-sprite/sprite-sheet-frames.h>

namespace eng {

namespace {

  /// Cells @p sheet's grid holds, never zero: a sheet with no columns or no
  /// rows is one a hand-edited file left blank, and it shows one frame.
  uint32_t gridCells(const SpriteSheet& sheet) {
    const auto columns =
        static_cast<uint32_t>(std::max<uint16_t>(sheet.columns, 1));
    const auto rows = static_cast<uint32_t>(std::max<uint16_t>(sheet.rows, 1));
    return columns * rows;
  }

}  // namespace

uint16_t spriteSheetFrameCount(const SpriteSheet& sheet) {
  const uint32_t cells = gridCells(sheet);
  const uint32_t asked = sheet.frames == 0 ? cells : sheet.frames;
  return static_cast<uint16_t>(std::min(asked, cells));
}

uint16_t spriteFrameAt(const SpriteSheet& sheet, double seconds) {
  const uint16_t count = spriteSheetFrameCount(sheet);
  if (sheet.fps <= 0.0f || seconds <= 0.0) {
    return 0;
  }
  // Through a double and a modulus rather than by accumulating a frame
  // index: a clock that has been running for an hour still lands on the
  // frame its own time says, with no drift to have accumulated.
  const double played = std::floor(seconds * static_cast<double>(sheet.fps));
  return static_cast<uint16_t>(std::fmod(played, static_cast<double>(count)));
}

SpriteUvRect spriteFrameUv(const SpriteSheet& sheet, uint16_t frame) {
  const auto columns =
      static_cast<uint32_t>(std::max<uint16_t>(sheet.columns, 1));
  const uint32_t wrapped = frame % spriteSheetFrameCount(sheet);
  const auto width = 1.0f / static_cast<float>(columns);
  const auto height =
      1.0f / static_cast<float>(std::max<uint16_t>(sheet.rows, 1));
  // The two are whole numbers of cells before they are anything else:
  // dividing after the cast would put a frame between two rows.
  const uint32_t across = wrapped % columns;
  const uint32_t down = wrapped / columns;
  const auto column = static_cast<float>(across);
  const auto row = static_cast<float>(down);
  return {column * width, row * height, (column + 1.0f) * width,
          (row + 1.0f) * height};
}

Vec2 spriteFramePixels(const SpriteSheet& sheet, Vec2 pixels) {
  const auto columns = static_cast<float>(std::max<uint16_t>(sheet.columns, 1));
  const auto rows = static_cast<float>(std::max<uint16_t>(sheet.rows, 1));
  return {pixels.x / columns, pixels.y / rows};
}

SpriteUvRect insetSpriteUv(const SpriteUvRect& uv, float u_texel,
                           float v_texel) {
  const float u = u_texel * 0.5f;
  const float v = v_texel * 0.5f;
  return {uv.u0 + u, uv.v0 + v, uv.u1 - u, uv.v1 - v};
}

}  // namespace eng

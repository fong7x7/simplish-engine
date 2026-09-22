#include <algorithm>
#include <editor/shell/editor-sprite-quad-key.h>
#include <engine/render-sprite/sprite-sheet-frames.h>

namespace eng::editor {

namespace {

  /// @p value as a texel count, held to what the key can carry.
  uint16_t asTexels(float value) {
    return static_cast<uint16_t>(std::clamp(value, 0.0f, 65535.0f));
  }

}  // namespace

EditorSpriteQuadKey makeEditorSpriteQuadKey(const SpriteSheet& grid,
                                            uint16_t frame, Vec2 pixels) {
  return {grid.columns, grid.rows, frame, asTexels(pixels.x),
          asTexels(pixels.y)};
}

SpriteUvRect editorSpriteQuadUv(const EditorSpriteQuadKey& key) {
  const SpriteSheet grid{key.columns, key.rows, 0, 0.0f};
  const SpriteUvRect cell = spriteFrameUv(grid, key.frame);
  if (key.pixel_width == 0 || key.pixel_height == 0) {
    return cell;
  }
  return insetSpriteUv(cell, 1.0f / static_cast<float>(key.pixel_width),
                       1.0f / static_cast<float>(key.pixel_height));
}

}  // namespace eng::editor

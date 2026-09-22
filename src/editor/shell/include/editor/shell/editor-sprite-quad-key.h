#pragma once

/// @file editor-sprite-quad-key.h
/// @brief What decides the geometry one frame of a sprite sheet is drawn on.
/// @par Threading Thread-safe (immutable value type).

#include <compare>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/render-sprite/sprite-sheet.h>
#include <engine/render-sprite/sprite-uv-rect.h>

namespace eng::editor {

/// Everything a billboard's quad depends on.
///
/// A frame is drawn by giving the mesh pipeline a four-vertex quad carrying
/// that frame's texture coordinates, so the editor uploads one quad per
/// frame it actually shows and keeps them. This is the key it keeps them
/// under, and the reason each field is in it: the grid and the frame decide
/// which cell of the sheet the quad covers, and the sheet's pixel size
/// decides how far the coordinates are pulled in off the seam.
///
/// Two billboards on the same sheet with the same grid therefore share
/// every quad, however many of them there are, and a sheet playing at
/// twelve frames a second uploads nothing after its first loop.
/// @thread_safety Immutable value type.
struct EditorSpriteQuadKey {
  /// Frames across the sheet.
  uint16_t columns = 1;
  /// Frames down the sheet.
  uint16_t rows = 1;
  /// Which frame, counted left to right and then down.
  uint16_t frame = 0;
  /// The sheet image's width in texels.
  uint16_t pixel_width = 0;
  /// The sheet image's height in texels.
  uint16_t pixel_height = 0;

  /// Ordered so the editor can keep quads in a sorted map, which is one
  /// fewer hash to have an opinion about ([ADR-002]).
  [[nodiscard]] auto operator<=>(const EditorSpriteQuadKey&) const = default;
  /// Equality, as the comparison implies.
  [[nodiscard]] bool operator==(const EditorSpriteQuadKey&) const = default;
};

/// The key for @p frame of @p grid, on a sheet @p pixels big.
[[nodiscard]] EditorSpriteQuadKey
makeEditorSpriteQuadKey(const SpriteSheet& grid, uint16_t frame, Vec2 pixels);

/// The texture coordinates the quad @p key names covers: the frame's own
/// cell, pulled in half a texel so bilinear sampling cannot reach the cell
/// beside it. A sheet of no known size is not pulled in at all, there being
/// no texel to measure.
[[nodiscard]] SpriteUvRect editorSpriteQuadUv(const EditorSpriteQuadKey& key);

}  // namespace eng::editor

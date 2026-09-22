#pragma once

/// @file sprite-sheet.h
/// @brief How a sprite sheet image is cut into frames, and how fast they run.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng {

/// The grid a sprite sheet is cut into, and the speed its frames play at.
///
/// A grid rather than a list of rectangles: sheets are exported from every
/// tool as an even grid, and a grid is four numbers a designer can type
/// where a rectangle list is a file format of its own
/// ([ADR-003](../../../../../docs/decisions/ADR-003-hybrid-iso-render-model.md)).
/// The image itself is not here — this describes any sheet, and the same
/// description is what a level file saves.
/// @thread_safety Immutable value type.
struct SpriteSheet {
  /// Frames across the image.
  uint16_t columns = 1;
  /// Frames down the image.
  uint16_t rows = 1;
  /// How many of those cells hold a frame, counted left to right and then
  /// down. Zero means every cell of the grid, which is what a sheet whose
  /// last row is full wants and what a dropped billboard starts at.
  ///
  /// Separate from the grid because the two disagree often: a 4x3 sheet of
  /// ten frames has two empty cells at the end, and playing them would
  /// blink the sprite out twice a loop.
  uint16_t frames = 0;
  /// Frames a second. Zero holds the sheet on its first frame, which is
  /// what a sheet of facings rather than of animation wants.
  float fps = 12.0f;
};

}  // namespace eng

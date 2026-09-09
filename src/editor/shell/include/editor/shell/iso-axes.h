#pragma once

/// @file iso-axes.h
/// @brief The screen distances the three world axes cover, per projection.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/project/project-projection.h>

namespace eng::editor {

/// Width of one tile's screen footprint at zoom 1.0, in logical pixels.
///
/// Shared by both projections: a tile is 64 pixels across either way, which
/// is what keeps a tileset's pixel budget the same whichever one a project
/// picks.
inline constexpr float ISO_TILE_WIDTH = 64.0f;

/// Square root usable in a constant expression.
///
/// `std::sqrt` is not constexpr before C++26 and these axes are constants,
/// so Newton's method stands in. It converges on a float in far fewer than
/// the iterations below; the count is chosen to be obviously enough rather
/// than to be tight.
[[nodiscard]] constexpr float isoSqrt(float value) {
  if (value <= 0.0f) {
    return 0.0f;
  }
  float guess = value < 1.0f ? 1.0f : value;
  for (int i = 0; i < 32; ++i) {
    guess = 0.5f * (guess + value / guess);
  }
  return guess;
}

/// The height scale that makes a projection a rotation of the world rather
/// than a shear of it.
///
/// A projection is a 2x3 matrix: one row taking a world point to screen X,
/// one to screen Y. It preserves shape — circles stay circles, spheres draw
/// round — exactly when those two rows are perpendicular and the same
/// length. The ground axes settle the first row and the perpendicularity;
/// what is left is the length, and solving the second row's length for the
/// height term gives this.
///
/// Get it wrong and nothing looks obviously broken: tiles, grid and cubes
/// all still line up, because they are drawn from the same axes. Only a
/// round thing gives it away, by drawing as an oval.
[[nodiscard]] constexpr float isoRiseFor(float x_across, float y_across,
                                         float x_down, float y_down) {
  return isoSqrt(x_across * x_across + y_across * y_across - x_down * x_down -
                 y_down * y_down);
}

/// Screen Y per +1 world Y under the dimetric axes: tiles 4:3.
inline constexpr float ISO_DIMETRIC_DEPTH = ISO_TILE_WIDTH * 0.75f;
/// Screen Y per +1 world Z under the dimetric axes. Derived, not chosen:
/// see `isoRiseFor`. Works out at 42.33 px, the cosine of a 48.6 degree
/// camera pitch times the tile width.
inline constexpr float ISO_DIMETRIC_RISE =
    isoRiseFor(ISO_TILE_WIDTH, 0.0f, 0.0f, ISO_DIMETRIC_DEPTH);

/// Screen X per +1 world X under the isometric axes: half a tile across.
inline constexpr float ISO_ISOMETRIC_ACROSS = ISO_TILE_WIDTH * 0.5f;
/// Screen Y per +1 world X under the isometric axes: a quarter down, which
/// is what makes the tile a 2:1 diamond.
inline constexpr float ISO_ISOMETRIC_DOWN = ISO_TILE_WIDTH * 0.25f;
/// Screen Y per +1 world Z under the isometric axes. Derived the same way,
/// and 39.19 px — the 30 degree pitch a 2:1 diamond implies.
inline constexpr float ISO_ISOMETRIC_RISE =
    isoRiseFor(ISO_ISOMETRIC_ACROSS, -ISO_ISOMETRIC_ACROSS, ISO_ISOMETRIC_DOWN,
               ISO_ISOMETRIC_DOWN);

/// How the world axes land on the screen plane.
///
/// Five numbers, because a projection is a linear map: each world axis
/// contributes to screen X and screen Y. Holding it as data rather than as
/// constants is what lets a project choose between projections without a
/// second copy of every routine that draws or picks.
/// @thread_safety Immutable value type.
struct IsoAxes {
  /// Screen X covered by +1 world X.
  float x_across = ISO_TILE_WIDTH;
  /// Screen X covered by +1 world Y. Zero unless the camera has yaw.
  float y_across = 0.0f;
  /// Screen Y covered by +1 world X, downward. Zero unless it has yaw.
  float x_down = 0.0f;
  /// Screen Y covered by +1 world Y, downward.
  float y_down = ISO_DIMETRIC_DEPTH;
  /// Screen Y covered by +1 world Z, upward. Not free: `isoRiseFor` is what
  /// it has to be for the projection to keep shapes.
  float z_up = ISO_DIMETRIC_RISE;
};

/// Zero yaw: axis-aligned 64 by 48 tiles, seen from a camera pitched far
/// enough down to show that much ground. Vertical faces are seen head-on,
/// and height is foreshortened by the pitch's cosine like everything else a
/// camera sees.
inline constexpr IsoAxes ISO_AXES_DIMETRIC{
    ISO_TILE_WIDTH, 0.0f, 0.0f, ISO_DIMETRIC_DEPTH, ISO_DIMETRIC_RISE};

/// 45 degrees of yaw at a 2:1 pitch: 64 by 32 diamond tiles, the look most
/// games mean by isometric. Both ground axes run diagonally, half a tile
/// across and a quarter down each.
inline constexpr IsoAxes ISO_AXES_ISOMETRIC{
    ISO_ISOMETRIC_ACROSS, -ISO_ISOMETRIC_ACROSS, ISO_ISOMETRIC_DOWN,
    ISO_ISOMETRIC_DOWN, ISO_ISOMETRIC_RISE};

/// The axes a project's chosen projection draws with.
[[nodiscard]] constexpr IsoAxes isoAxesFor(ProjectProjection projection) {
  return projection == ProjectProjection::ISOMETRIC ? ISO_AXES_ISOMETRIC
                                                    : ISO_AXES_DIMETRIC;
}

}  // namespace eng::editor

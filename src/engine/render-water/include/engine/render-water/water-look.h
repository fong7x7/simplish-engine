#pragma once

/// @file water-look.h
/// @brief How painted water takes the light: what it reflects, its foam,
/// and how clear it is.
/// @par Threading A value type.

#include <engine/math/vec3.h>

namespace eng {

/// What the water shader mixes beyond each cell's own colour and opacity
/// (`WaterCell`), as sRGB colours from 0 to 1, the way a palette authors
/// them.
///
/// Looking down into water `d` tiles deep, its colour hides
/// `1 − exp(−absorption × d)` of the ground under it, where the
/// absorption runs from `clear_absorption` at opacity 0 to
/// `murky_absorption` at opacity 1, evenly on a log scale — so a clear
/// lake still shows its bed and a murky puddle already hides half of
/// its. The colour itself darkens by up to `deep_darkening` over
/// `colour_depth` tiles.
struct WaterLook {
  /// What the surface reflects at a glancing angle.
  Vec3 sky{0.70f, 0.82f, 0.92f};
  /// Foam on the shoreline and on crests.
  Vec3 foam{0.93f, 0.96f, 0.97f};
  /// How much a tile of the clearest water hides, as an absorption.
  float clear_absorption = 0.15f;
  /// How much a tile of the most opaque water hides.
  float murky_absorption = 12.0f;
  /// Tiles of depth over which the water's colour darkens.
  float colour_depth = 1.5f;
  /// How much darker deep water is than its own colour, 0 to 1.
  float deep_darkening = 0.55f;
};

}  // namespace eng

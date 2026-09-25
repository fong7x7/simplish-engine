#pragma once

/// @file water-obstacle.h
/// @brief Something standing in the water, which its ripples go round.
/// @par Threading A value type.

#include <engine/math/vec2.h>

namespace eng {

/// The footprint of something standing in the water — a crate, a pillar,
/// a rock — as a box on the ground, in tiles. The field is dry under it:
/// ripples meet it as they meet a shore, reflecting off it and breaking
/// into foam round its foot, and the shore's lapping reaches it too.
struct WaterObstacle {
  /// Its south-west corner.
  Vec2 min{};
  /// Its north-east corner.
  Vec2 max{};

  /// Whether two footprints are the same.
  bool operator==(const WaterObstacle& other) const {
    return min.x == other.min.x && min.y == other.min.y &&
           max.x == other.max.x && max.y == other.max.y;
  }
};

}  // namespace eng

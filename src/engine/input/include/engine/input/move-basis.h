#pragma once

/// @file move-basis.h
/// @brief Which way the screen's right and down point on the ground.
/// @par Threading
/// A value type.

#include <engine/math/vec2.h>

namespace eng::input {

/// The world directions — X and Y on the ground — that the screen's right
/// and down point along, which is what makes movement camera-relative.
///
/// A player pushing up means up the screen, whatever the camera's yaw: under
/// a zero-yaw view that is world -Y, and under a 45° isometric one it is a
/// diagonal across the grid. The keys are read in screen directions and
/// turned into world directions through this, before the input is
/// quantised; the simulation, the lockstep wire and replays only ever see
/// world directions, so none of them depends on how the level is drawn.
///
/// Both vectors are unit length and perpendicular — the camera rotates the
/// ground, it never shears it — so a full push is a full-speed move in any
/// direction. The default is the zero-yaw view, where screen and world axes
/// agree.
struct MoveBasis {
  /// World direction of screen-right, unit length.
  Vec2 right{1.0F, 0.0F};
  /// World direction of screen-down, unit length.
  Vec2 down{0.0F, 1.0F};
};

}  // namespace eng::input

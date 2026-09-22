#pragma once

/// @file fx-quad-view.h
/// @brief The camera and surface particle quads are laid out for.
/// @par Threading
/// A value type.

#include <engine/math/mat4.h>
#include <engine/render-mesh/mesh-light.h>
#include <span>

namespace eng {

/// What laying a particle out on screen needs: the matrix the scene was
/// drawn with, and the size of the surface it maps onto, so a particle is
/// round in pixels rather than in clip space.
struct FxQuadView {
  /// World-to-clip matrix the scene was drawn with.
  Mat4 view_projection{};
  /// Width of the surface the matrix maps onto, in pixels.
  float width = 0.0f;
  /// Height of that surface, in pixels.
  float height = 0.0f;
  /// The lights the scene is drawn by, which a particle whose look is
  /// `LIT` takes its colour from. Empty leaves every particle its own
  /// colour, whatever its look says.
  std::span<const MeshLight> lights{};
};

}  // namespace eng

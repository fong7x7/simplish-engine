#pragma once

/// @file fx-particle-lighting.h
/// @brief Whether a particle makes its own light or takes the scene's.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng {

/// Where a particle's colour comes from.
///
/// Worked out once per particle as its quad is laid out, not per pixel: a
/// particle is small, its lighting barely changes across one, and this way
/// the shaders stay the same in all four backends.
/// @thread_safety Immutable value type.
enum class FxParticleLighting : uint8_t {
  /// Its colour is its own, whatever the scene is lit by: a spark, a
  /// flame, a flash. The look every particle had before smoke.
  EMISSIVE,
  /// Its colour is dimmed and tinted by the lights near it, so smoke is
  /// dark in a dark room and lit orange by the blast that threw it.
  LIT,
};

}  // namespace eng

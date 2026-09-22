#pragma once

/// @file fx-particle-shape.h
/// @brief What shape a particle is drawn as.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng {

/// The two shapes a particle's quad is filled with.
///
/// Both are worked out in the fragment stage rather than sampled from a
/// sheet: the project has no art pipeline for effects yet, and a puff broken
/// up by noise costs a few instructions where a texture would cost an
/// atlas, a binding every backend has to carry, and a file to ship.
/// @thread_safety Immutable value type.
enum class FxParticleShape : uint8_t {
  /// A soft round disc: a spark, an ember, the core of a flash.
  DISC,
  /// A disc broken up by noise, turned by its own angle and different in
  /// every particle — what smoke and dust are drawn as.
  PUFF,
};

}  // namespace eng

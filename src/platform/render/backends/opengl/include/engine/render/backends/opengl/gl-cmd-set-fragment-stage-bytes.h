#pragma once

#include <cstdint>

namespace eng::render {

/// A block of bytes for the fragment stage, recorded inline.
///
/// The mesh shader's light block is the one thing that uses this, and it is
/// the largest inline payload the backend carries: a count and eight lights
/// of three `vec4` each. Held by value in the command stream, as every
/// other GL command is, so a recorded list does not point at caller memory
/// that may be gone by the time it is executed.
struct GlCmdSetFragmentStageBytes {
  /// Matches RHI `setFragmentStageBytes` slot index.
  uint32_t slot = 0;
  /// Byte count (`data` prefix).
  uint32_t size = 0;
  /// Inline payload: one 16-byte header and eight 48-byte lights.
  uint8_t data[416]{};
};

}  // namespace eng::render

#pragma once

/// @file footstep-surface.h
/// @brief What a character is walking on, as far as its feet can hear.
/// @par Threading
/// A value type.

#include <array>
#include <cstddef>
#include <cstdint>

namespace eng::game {

/// The material under a character's feet, which with its step set picks
/// the sound each step makes (`footstep-sounds.h`).
///
/// Materials rather than terrains: several things a level is painted or
/// built with can sound alike — a road and a paved floor are both stone —
/// and a prop laid on the ground can be a surface no terrain is, such as a
/// wooden deck or a rug. Presentation — the simulation never reads it.
enum class FootstepSurface : uint8_t {
  /// Bare ground: nothing painted and nothing laid there.
  GROUND,
  /// Grass and undergrowth.
  GRASS,
  /// Packed earth and mud.
  DIRT,
  /// Loose sand and gravel.
  SAND,
  /// Shallow water.
  WATER,
  /// Stone, concrete and road.
  STONE,
  /// Boards and decking.
  WOOD,
  /// Grates, plates and walkways.
  METAL,
  /// Rugs and carpet.
  CLOTH,
};

/// How many surfaces there are.
inline constexpr size_t FOOTSTEP_SURFACE_COUNT = 9;

/// Every surface, in the order a choice row steps through them.
inline constexpr std::array<FootstepSurface, FOOTSTEP_SURFACE_COUNT>
    ALL_FOOTSTEP_SURFACES{FootstepSurface::GROUND, FootstepSurface::GRASS,
                          FootstepSurface::DIRT,   FootstepSurface::SAND,
                          FootstepSurface::WATER,  FootstepSurface::STONE,
                          FootstepSurface::WOOD,   FootstepSurface::METAL,
                          FootstepSurface::CLOTH};

}  // namespace eng::game

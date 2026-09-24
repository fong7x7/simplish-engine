#pragma once

/// @file water-fidelity.h
/// @brief How much work painted water is drawn with.
/// @par Threading Thread-safe (immutable data and pure functions).

#include <cstdint>
#include <optional>
#include <string_view>

namespace eng {

/// How water is drawn: from a still, translucent surface to one simulated
/// finely enough to show a wake.
///
/// A graphics setting, not a property of the level: the same water is
/// painted at every fidelity, and nothing about it reaches a tick.
enum class WaterFidelity : uint8_t {
  /// A still surface: the water's depth, colour and clarity over the ground,
  /// its shore foam and the lights' glints, with nothing simulated and no
  /// waves, so it costs one draw and nothing a frame on the CPU.
  FLAT,
  /// A simulated surface at `WATER_LOW_SAMPLES_PER_TILE`, shaded by its
  /// ripples, its depth and the sky, with no fine detail.
  LOW,
  /// A simulated surface at `WATER_HIGH_SAMPLES_PER_TILE`, with wind waves
  /// finer than the simulation, light caught in the shallows and foam on
  /// the crests.
  HIGH,
};

/// Every fidelity, lowest first.
inline constexpr WaterFidelity WATER_FIDELITIES[] = {
    WaterFidelity::FLAT, WaterFidelity::LOW, WaterFidelity::HIGH};

/// The fidelity water is drawn at when nobody has chosen one.
inline constexpr WaterFidelity WATER_DEFAULT_FIDELITY = WaterFidelity::HIGH;

/// Samples along each side of a tile a `FLAT` surface is shaped at: enough
/// for its banks, never stepped.
inline constexpr uint32_t WATER_FLAT_SAMPLES_PER_TILE = 2;

/// Simulated samples along each side of a tile at `LOW`.
inline constexpr uint32_t WATER_LOW_SAMPLES_PER_TILE = 4;

/// Simulated samples along each side of a tile at `HIGH`.
inline constexpr uint32_t WATER_HIGH_SAMPLES_PER_TILE = 8;

/// Whether @p fidelity moves the water at all.
[[nodiscard]] constexpr bool waterFidelitySimulates(WaterFidelity fidelity) {
  return fidelity != WaterFidelity::FLAT;
}

/// Samples along each side of a tile @p fidelity shapes the water at:
/// `FLAT`'s only for its banks, the others' to simulate.
[[nodiscard]] constexpr uint32_t waterSamplesPerTile(WaterFidelity fidelity) {
  switch (fidelity) {
    case WaterFidelity::LOW:
      return WATER_LOW_SAMPLES_PER_TILE;
    case WaterFidelity::HIGH:
      return WATER_HIGH_SAMPLES_PER_TILE;
    case WaterFidelity::FLAT:
      break;
  }
  return WATER_FLAT_SAMPLES_PER_TILE;
}

/// The word @p fidelity is named by, in a settings file and to the agent
/// API: `flat`, `low` or `high`.
[[nodiscard]] constexpr std::string_view
waterFidelityWord(WaterFidelity fidelity) {
  switch (fidelity) {
    case WaterFidelity::LOW:
      return "low";
    case WaterFidelity::HIGH:
      return "high";
    case WaterFidelity::FLAT:
      break;
  }
  return "flat";
}

/// The fidelity @p word names, or nothing for a word none has.
[[nodiscard]] constexpr std::optional<WaterFidelity>
waterFidelityNamed(std::string_view word) {
  for (const WaterFidelity fidelity : WATER_FIDELITIES) {
    if (waterFidelityWord(fidelity) == word) {
      return fidelity;
    }
  }
  return std::nullopt;
}

}  // namespace eng

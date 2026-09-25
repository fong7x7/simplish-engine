#pragma once

/// @file water-effect.h
/// @brief The parts of drawing water that can be switched off by
/// themselves.
/// @par Threading Thread-safe (immutable data and pure functions).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace eng {

/// One part of drawing water a user can switch off whatever the fidelity:
/// the ones that read the scene per pixel, and so cost the most on a slow
/// machine, and the caustics. Each one off is skipped, not merely hidden.
/// A graphics setting, as the fidelity is.
enum class WaterEffect : uint8_t {
  /// The scene mirrored in the water: a ray followed through its depth.
  REFLECTIONS,
  /// The ground seen through the water bent by its ripples.
  REFRACTION,
  /// Foam rings and shadows at the foot of whatever stands in the water.
  CONTACT,
  /// Threads of light the waves focus on the ground under the water.
  CAUSTICS,
};

/// Every effect, in the order `WaterEffects` holds them.
inline constexpr WaterEffect WATER_EFFECT_LIST[] = {
    WaterEffect::REFLECTIONS, WaterEffect::REFRACTION, WaterEffect::CONTACT,
    WaterEffect::CAUSTICS};

/// How many effects there are.
inline constexpr size_t WATER_EFFECT_COUNT = std::size(WATER_EFFECT_LIST);

/// The word each effect is named by, in a settings file and to the agent
/// API, in `WATER_EFFECT_LIST`'s order.
inline constexpr std::string_view WATER_EFFECT_WORDS[] = {
    "reflections", "refraction", "contact", "caustics"};

static_assert(std::size(WATER_EFFECT_WORDS) == WATER_EFFECT_COUNT,
              "every water effect needs a word");

/// Where @p effect is in `WATER_EFFECT_LIST` and `WaterEffects`.
[[nodiscard]] constexpr size_t waterEffectIndex(WaterEffect effect) {
  return static_cast<size_t>(effect);
}

/// The word @p effect is named by.
[[nodiscard]] constexpr std::string_view waterEffectWord(WaterEffect effect) {
  return WATER_EFFECT_WORDS[waterEffectIndex(effect)];
}

/// The effect @p word names, or nothing.
[[nodiscard]] constexpr std::optional<WaterEffect>
waterEffectNamed(std::string_view word) {
  for (const WaterEffect effect : WATER_EFFECT_LIST) {
    if (waterEffectWord(effect) == word) {
      return effect;
    }
  }
  return std::nullopt;
}

}  // namespace eng

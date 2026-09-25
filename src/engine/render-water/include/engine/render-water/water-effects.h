#pragma once

/// @file water-effects.h
/// @brief Which of the water's switchable effects are drawn.
/// @par Threading A value type.

#include <array>
#include <engine/render-water/water-effect.h>

namespace eng {

/// Which of the `WaterEffect`s are drawn, by `waterEffectIndex`. Every one
/// is on until the user switches it off.
struct WaterEffects {
  /// Whether each effect is drawn.
  std::array<bool, WATER_EFFECT_COUNT> on{true, true, true, true};

  /// Two sets are the same when every switch is.
  bool operator==(const WaterEffects&) const = default;
};

/// Whether @p effects draws @p effect.
[[nodiscard]] constexpr bool waterEffectOn(const WaterEffects& effects,
                                           WaterEffect effect) {
  return effects.on[waterEffectIndex(effect)];
}

}  // namespace eng

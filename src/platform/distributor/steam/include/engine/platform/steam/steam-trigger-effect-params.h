#pragma once

#include <cstdint>

namespace eng {

enum class SteamTriggerEffectMode : uint8_t {
  OFF,
  FEEDBACK,
  WEAPON,
  VIBRATION,
};

struct SteamTriggerEffectParams {
  /// Start position on the trigger (0-9).
  uint8_t position = 0;
  /// Effect strength (0-8).
  uint8_t strength = 0;
  /// Vibration frequency (only for VIBRATION mode).
  uint8_t frequency = 0;
};

}  // namespace eng

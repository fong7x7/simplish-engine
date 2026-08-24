#pragma once

#include <cstdint>

namespace eng {

enum class Ps5TriggerSide : uint8_t {
  LEFT,
  RIGHT,
};

enum class Ps5TriggerMode : uint8_t {
  OFF,
  FEEDBACK,
  VIBRATION,
  WEAPON,
};

struct Ps5TriggerEffectParams {
  /// Trigger position where the effect begins [0.0, 1.0].
  float start = 0.0f;
  /// Trigger position where the effect ends [0.0, 1.0].
  float end = 1.0f;
  /// Effect strength [0.0, 255.0].
  float force = 0.0f;
  /// Vibration frequency (only used for VIBRATION mode).
  float frequency = 0.0f;
};

}  // namespace eng

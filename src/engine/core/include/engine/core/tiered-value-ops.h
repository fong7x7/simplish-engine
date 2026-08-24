#pragma once

/// @file tiered-value-ops.h
/// @brief Static operations on TieredValue.
/// @threading Main-thread only.

#include <engine/core/tier-definition.h>
#include <engine/core/tiered-value.h>
#include <vector>

namespace eng {

/// Static operations on TieredValue. All functions are pure —
/// they operate only on their parameters with no hidden state.
struct TieredValueOps {
  /// Initialize with tier definitions and starting value.
  /// Asserts tiers is non-empty. Clamps initial_value to range.
  static void init(TieredValue& tv, const std::vector<TierDefinition>& tiers,
                   float initial_value);

  /// Set the value. Clamps to overall range. Returns TieredValue::ChangeResult.
  static TieredValue::ChangeResult set(TieredValue& tv, float new_value);

  /// Get the current value.
  static float get(const TieredValue& tv);

  /// Get the current tier index (0-based).
  static uint8_t getTierIndex(const TieredValue& tv);

  /// Get the number of tiers.
  static uint8_t tierCount(const TieredValue& tv);

  /// Adjust value by delta. Equivalent to set(get(tv) + delta).
  static TieredValue::ChangeResult adjust(TieredValue& tv, float delta);
};

}  // namespace eng

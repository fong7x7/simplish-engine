#pragma once

#include <cstdint>
#include <engine/core/tier-definition.h>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// TieredValue: A clamped numeric value mapped to discrete tiers with
// tier-change detection via return values.
//
// Responsibilities:
// - Store a numeric value clamped to the overall tier range
// - Track the current tier index based on value
// - Return TieredValue::ChangeResult from set/adjust for caller-driven
// notification
//
// Key Invariants:
// - Value is always clamped to [tiers.front().value_min,
// tiers.back().value_max]
// - current_tier_index always reflects the tier containing the current value
// - Only value is persisted; tier index is recomputed from value + definitions
// - Tiers must be non-empty and ordered ascending by value_min
//
// Thread Safety:
// - Main-thread only. No synchronization needed.
//
// ADR Reference:
// - ADR-011: Return-based tier notification (no stored callbacks)
// ============================================================================

/// A clamped numeric value with discrete tier boundaries.
struct TieredValue {
  /// Result of a TieredValue set/adjust operation.
  /// Caller checks previous_tier_index != new_tier_index for tier transitions.
  struct ChangeResult {
    /// Value before the change.
    float previous_value = 0.0f;
    /// Value after the change (clamped to tier range).
    float new_value = 0.0f;
    /// Tier index before the change.
    uint8_t previous_tier_index = 0;
    /// Tier index after the change.
    uint8_t new_tier_index = 0;
  };
  /// Current numeric value (clamped to overall tier range).
  float value = 0.0f;
  /// Index of the current tier (0-based into tiers vector).
  uint8_t current_tier_index = 0;
  /// Ordered tier definitions (ascending by value_min).
  std::vector<TierDefinition> tiers;
};

}  // namespace eng

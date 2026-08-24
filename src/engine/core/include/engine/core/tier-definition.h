#pragma once

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// TierDefinition: A single tier boundary in a tiered value system.
//
// Responsibilities:
// - Define the inclusive value range [value_min, value_max] for one tier
//
// Key Invariants:
// - value_min <= value_max
// - Tiers are stored in ascending order by value_min in TieredValue
// - Tier index (position in the vector) serves as the tier identifier
//
// Thread Safety:
// - Immutable after initialization. Safe to read from any thread.
// ============================================================================

/// A single tier boundary defining an inclusive value range.
struct TierDefinition {
  /// Inclusive lower bound for this tier.
  float value_min = 0.0f;
  /// Inclusive upper bound for this tier.
  float value_max = 0.0f;
};

}  // namespace eng

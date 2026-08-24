#pragma once

#include <cstdint>
#include <engine/core/modifier.h>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// ModifierStack: An ordered collection of modifiers with priority ordering,
// source tracking, duration-based expiry, and deterministic computation.
//
// Responsibilities:
// - Store modifiers in priority-sorted order (stable within same priority)
// - Add/remove modifiers by source
// - Tick durations and expire modifiers
// - Compute final value from base + all modifiers
//
// Key Invariants:
// - Modifiers maintained in ascending priority order (stable insertion)
// - compute() applies: flat/add sum -> percent -> multiply -> override
// - Stack respects max_modifiers capacity from config
//
// Thread Safety:
// - Main-thread only. No synchronization needed.
// ============================================================================

/// Ordered collection of modifiers. Primary data structure.
struct ModifierStack {
  /// Configuration for modifier stack capacity limits.
  struct Config {
    /// Maximum number of modifiers allowed per stack.
    uint32_t max_modifiers = 64;
  };
  /// Active modifiers, maintained in ascending priority order.
  std::vector<Modifier> modifiers;
};

}  // namespace eng

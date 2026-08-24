#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Modifier: A single numeric modifier entry for the modifier stack framework.
//
// Responsibilities:
// - Represent a modifier with mode, priority, value, source, and duration
// - Provide the ModifierMode enum for application formula selection
//
// Key Invariants:
// - Priority is a uint8_t; lower values are applied first
// - Duration is std::nullopt for permanent (never expires)
// - Source identifies the modifier origin for add/remove tracking
//
// Thread Safety:
// - Main-thread only. No synchronization needed.
// ============================================================================

/// Application mode determining how a modifier's value combines with the base.
enum class ModifierMode : uint8_t {
  FLAT = 0,      // base + value (additive offset)
  PERCENT = 1,   // (base + flat_sum) * (1 + value) (percentage scaling)
  MULTIPLY = 2,  // result * value (multiplicative scaling)
  ADD = 3,       // base + value (same formula as FLAT, distinct semantic)
  OVERRIDE = 4,  // replaces computed value; last override wins
};

/// A single modifier entry stored in a ModifierStack.
struct Modifier {
  /// Source identifier (e.g. "effect:strength_potion", "slot:barrel").
  std::string source;
  /// How this modifier's value combines with the base during compute.
  ModifierMode mode = ModifierMode::FLAT;
  /// Priority bucket for application ordering (lower = applied first).
  uint8_t priority = 0;
  /// Modifier magnitude.
  float value = 0.0f;
  /// Seconds remaining; std::nullopt means permanent (no expiry).
  std::optional<float> duration_s;
};

}  // namespace eng

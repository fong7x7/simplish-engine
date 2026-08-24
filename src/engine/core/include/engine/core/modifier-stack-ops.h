#pragma once

/// @file modifier-stack-ops.h
/// @brief Static operations on ModifierStack.
/// @threading Main-thread only.

#include <cstdint>
#include <engine/core/modifier-stack.h>
#include <string_view>

namespace eng {

/// Static operations on ModifierStack. All functions are pure —
/// they operate only on their parameters with no hidden state.
struct ModifierStackOps {
  /// Add a modifier in priority-sorted position (stable insertion).
  /// Returns false and logs warning if stack is at max capacity.
  static bool add(const ModifierStack::Config& config, ModifierStack& stack,
                  const Modifier& modifier);

  /// Remove all modifiers matching the given source.
  /// Returns the count of modifiers removed.
  static uint32_t removeBySource(ModifierStack& stack, std::string_view source);

  /// Decrement durations by delta_s; remove expired modifiers.
  /// Returns the count of expired modifiers.
  static uint32_t tick(ModifierStack& stack, float delta_s);

  /// Compute final value: flat/add sum, percent, multiply, override.
  static float compute(const ModifierStack& stack, float base_value);

  /// Remove all modifiers from the stack.
  static void clear(ModifierStack& stack);

  /// Count modifiers matching the given source.
  static uint32_t countBySource(const ModifierStack& stack,
                                std::string_view source);
};

}  // namespace eng

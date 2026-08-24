#include <algorithm>
#include <engine/core/assert.h>
#include <engine/core/modifier-stack-ops.h>
#include <engine/core/modifier-stack.h>

namespace eng {

bool ModifierStackOps::add(const ModifierStack::Config& config,
                           ModifierStack& stack, const Modifier& modifier) {
  if (stack.modifiers.size() >= config.max_modifiers) {
    Logger::warn("ModifierStack", "at capacity; modifier rejected");
    return false;
  }
  auto pos = std::ranges::upper_bound(
      stack.modifiers, modifier.priority,
      [](uint8_t p, uint8_t q) { return p < q; }, &Modifier::priority);
  stack.modifiers.insert(pos, modifier);
  return true;
}

uint32_t ModifierStackOps::removeBySource(ModifierStack& stack,
                                          std::string_view source) {
  const auto old_size = stack.modifiers.size();
  auto [first, last] =
      std::ranges::remove_if(stack.modifiers, [source](const Modifier& m) {
        return m.source == source;
      });
  stack.modifiers.erase(first, last);
  return static_cast<uint32_t>(old_size - stack.modifiers.size());
}

uint32_t ModifierStackOps::tick(ModifierStack& stack, float delta_s) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(delta_s >= 0.0f, "tick delta must be non-negative");
  for (auto& mod : stack.modifiers) {
    if (mod.duration_s.has_value()) {
      *mod.duration_s -= delta_s;
    }
  }
  const auto old_size = stack.modifiers.size();
  auto [first, last] =
      std::ranges::remove_if(stack.modifiers, [](const Modifier& m) {
        return m.duration_s.has_value() && *m.duration_s <= 0.0f;
      });
  stack.modifiers.erase(first, last);
  return static_cast<uint32_t>(old_size - stack.modifiers.size());
}

// Named algorithm: apply modifiers to base value in deterministic order.
// Formula: (base + flat_sum) * (1 + pct_sum) * multiply_product, or override.
// Passes: 1) accumulate flat/add/pct sums, 2) apply multiply, 3) apply
// override.
float ModifierStackOps::compute(const ModifierStack& stack, float base_value) {
  float flat_sum = 0.0f;
  float pct_sum = 0.0f;
  for (const auto& mod : stack.modifiers) {
    if (mod.mode == ModifierMode::FLAT || mod.mode == ModifierMode::ADD) {
      flat_sum += mod.value;
    } else if (mod.mode == ModifierMode::PERCENT) {
      pct_sum += mod.value;
    }
  }
  float result = (base_value + flat_sum) * (1.0f + pct_sum);
  for (const auto& mod : stack.modifiers) {
    if (mod.mode == ModifierMode::MULTIPLY) {
      result *= mod.value;
    }
  }
  for (const auto& mod : stack.modifiers) {
    if (mod.mode == ModifierMode::OVERRIDE) {
      result = mod.value;
    }
  }
  return result;
}

void ModifierStackOps::clear(ModifierStack& stack) {
  stack.modifiers.clear();
}

uint32_t ModifierStackOps::countBySource(const ModifierStack& stack,
                                         std::string_view source) {
  return static_cast<uint32_t>(
      std::ranges::count_if(stack.modifiers, [source](const Modifier& m) {
        return m.source == source;
      }));
}

}  // namespace eng

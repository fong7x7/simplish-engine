#include <algorithm>
#include <engine/core/assert.h>
#include <engine/core/tiered-value-ops.h>
#include <engine/core/tiered-value.h>

namespace eng {

namespace {

  /// Clamp value to the overall tier range.
  float clampToRange(const std::vector<TierDefinition>& tiers, float val) {
    const float range_min = tiers.front().value_min;
    const float range_max = tiers.back().value_max;
    return std::clamp(val, range_min, range_max);
  }

  /// Find the tier index containing the given value.
  uint8_t findTierIndex(const std::vector<TierDefinition>& tiers, float val) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(tiers.size()); ++i) {
      if (val >= tiers[i].value_min && val <= tiers[i].value_max) {
        return i;
      }
    }
    return static_cast<uint8_t>(tiers.size() - 1);
  }

}  // namespace

void TieredValueOps::init(TieredValue& tv,
                          const std::vector<TierDefinition>& tiers,
                          float initial_value) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(!tiers.empty(), "tiers must be non-empty");
  tv.tiers = tiers;
  tv.value = clampToRange(tv.tiers, initial_value);
  tv.current_tier_index = findTierIndex(tv.tiers, tv.value);
}

TieredValue::ChangeResult TieredValueOps::set(TieredValue& tv,
                                              float new_value) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(!tv.tiers.empty(), "TieredValue not initialized");
  TieredValue::ChangeResult result{};
  result.previous_value = tv.value;
  result.previous_tier_index = tv.current_tier_index;
  tv.value = clampToRange(tv.tiers, new_value);
  tv.current_tier_index = findTierIndex(tv.tiers, tv.value);
  result.new_value = tv.value;
  result.new_tier_index = tv.current_tier_index;
  return result;
}

float TieredValueOps::get(const TieredValue& tv) {
  return tv.value;
}

uint8_t TieredValueOps::getTierIndex(const TieredValue& tv) {
  return tv.current_tier_index;
}

uint8_t TieredValueOps::tierCount(const TieredValue& tv) {
  return static_cast<uint8_t>(tv.tiers.size());
}

TieredValue::ChangeResult TieredValueOps::adjust(TieredValue& tv, float delta) {
  return set(tv, tv.value + delta);
}

}  // namespace eng

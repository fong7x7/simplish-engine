#include <engine/core/assert.h>
#include <engine/core/pcg32.h>

namespace eng {

namespace {

  /// The reference PCG32 LCG multiplier.
  constexpr uint64_t MULTIPLIER = 6364136223846793005ULL;

  /// 2^-24: scales a 24-bit integer into [0, 1) exactly.
  constexpr float UNIT_SCALE = 0x1.0p-24F;

}  // namespace

Pcg32::Pcg32(uint64_t seed, uint64_t stream) : increment_((stream << 1U) | 1U) {
  next();
  state_ += seed;
  next();
}

uint32_t Pcg32::next() {
  const uint64_t old = state_;
  state_ = old * MULTIPLIER + increment_;
  const auto xorshifted = static_cast<uint32_t>(((old >> 18U) ^ old) >> 27U);
  const auto rotation = static_cast<uint32_t>(old >> 59U);
  return (xorshifted >> rotation) | (xorshifted << ((32U - rotation) & 31U));
}

uint32_t Pcg32::nextBelow(uint32_t bound) {
  // ENGINE_ASSERT expands to the do-while statement-macro idiom.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(bound > 0, "Pcg32::nextBelow needs a bound above zero");
  // Values below the threshold would make the low residues more likely than
  // the high ones; rejecting them leaves a range that divides evenly.
  const uint32_t threshold = (0U - bound) % bound;
  uint32_t value = next();
  while (value < threshold) {
    value = next();
  }
  return value % bound;
}

float Pcg32::nextUnitFloat() {
  return static_cast<float>(next() >> 8U) * UNIT_SCALE;
}

}  // namespace eng

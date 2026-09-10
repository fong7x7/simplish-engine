#pragma once

/// @file pcg32.h
/// @brief PCG32, the one random number generator simulation code may use.
/// @par Threading
/// A value type. An instance is used by one thread at a time.

#include <cstdint>

namespace eng {

/// PCG32 — XSH-RR output over 64-bit LCG state — bit-identical to O'Neill's
/// reference `pcg32_random_r` on every platform (Engine REQUIREMENTS §4.3).
///
/// A generator is one *stream*. Two generators given the same seed and
/// different stream ids produce independent sequences, which is what lets
/// each simulation system own a stream (`spawn`, `loot`, `crit`, `fx`): a
/// system drawing one extra number never shifts another system's sequence.
///
/// Integer arithmetic throughout. `nextUnitFloat` is the one float result,
/// and it is exact — 24 random bits scaled by a power of two.
class Pcg32 {
public:
  /// Seeds stream `stream` from `seed`, as `pcg32_srandom_r` does.
  Pcg32(uint64_t seed, uint64_t stream);

  /// The next 32 random bits.
  uint32_t next();

  /// A uniform value in `[0, bound)`, without modulo bias. `bound` must be
  /// greater than zero.
  uint32_t nextBelow(uint32_t bound);

  /// A uniform value in `[0, 1)` with 24 bits of precision.
  float nextUnitFloat();

  /// The generator's state — hashed by the tick hash for simulation streams.
  [[nodiscard]] uint64_t state() const { return state_; }

  /// The stream's increment, fixed at seeding.
  [[nodiscard]] uint64_t increment() const { return increment_; }

  /// Two generators are equal when they will produce the same sequence.
  bool operator==(const Pcg32&) const = default;

private:
  /// LCG state; advanced once per `next`.
  uint64_t state_ = 0;
  /// Odd LCG increment; selects the stream.
  uint64_t increment_ = 1;
};

}  // namespace eng

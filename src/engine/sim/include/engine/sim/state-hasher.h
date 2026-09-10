#pragma once

/// @file state-hasher.h
/// @brief Folds simulation state into a 64-bit hash, byte for byte.
/// @par Threading
/// A value type; one instance per thread.

#include <cstddef>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <span>
#include <type_traits>

namespace eng::sim {

/// True for types whose bytes are exactly their value, so hashing the bytes
/// hashes the value. A struct with padding is false: its padding bytes are
/// indeterminate, and hashing them would make two identical states hash
/// differently.
///
/// Floats are included on purpose. The contract is bit-identical state
/// (ADR-002), so -0.0 and +0.0 hashing differently is correct, not a bug.
template <typename T>
inline constexpr bool IS_HASHABLE_BITS =
    std::has_unique_object_representations_v<T> || std::is_floating_point_v<T>;

/// `Vec2` is two floats with no padding.
template <>
inline constexpr bool IS_HASHABLE_BITS<Vec2> =
    sizeof(Vec2) == 2 * sizeof(float);

/// `Vec3` is three floats with no padding.
template <>
inline constexpr bool IS_HASHABLE_BITS<Vec3> =
    sizeof(Vec3) == 3 * sizeof(float);

/// A type `StateHasher::add` accepts. A game type of floats opts in by
/// specialising `IS_HASHABLE_BITS` beside a size check, as `Vec2` does.
template <typename T>
concept HashableBits = IS_HASHABLE_BITS<std::remove_cv_t<T>>;

/// A 64-bit hash over simulation state, for detecting divergence — between
/// peers, and between a replay and its recording (Engine REQUIREMENTS §4.3).
///
/// Word-at-a-time and order-sensitive. It is not a cryptographic hash and
/// does not need to be: its job is noticing that two runs differ, cheaply
/// enough to run every tick over 20,000 projectiles.
///
/// Defined over little-endian byte order, which every target has; the
/// implementation refuses to compile anywhere else.
class StateHasher {
public:
  /// Folds `bytes` in. Prefer `add` and `addSpan`, which reject padded types.
  void addBytes(std::span<const std::byte> bytes);

  /// Folds one value in.
  template <HashableBits T> void add(const T& value) {
    addBytes(std::as_bytes(std::span<const T, 1>(&value, 1)));
  }

  /// Folds a contiguous run of values in — one field array of a pool.
  template <HashableBits T> void addSpan(std::span<const T> values) {
    addBytes(std::as_bytes(values));
  }

  /// The hash of everything added so far.
  [[nodiscard]] uint64_t value() const { return state_; }

private:
  /// Running hash; starts at an arbitrary odd constant.
  uint64_t state_ = 0x9E3779B97F4A7C15ULL;
};

}  // namespace eng::sim

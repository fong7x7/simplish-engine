#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY -- Named Flag Registry
//
// Generic bitflag registry: plugins claim named flags at init, systems resolve
// names to bit indices at load time. Stores up to 64 flags in a uint64_t.
//
// Responsibilities:
//   - Map string names to bit indices (0-63)
//   - Prevent duplicate name claims
//   - Provide resolve (name -> bit index) for runtime lookup
//   - Provide inline helpers for bit manipulation
//
// Thread Safety:
//   - claim(): main-thread-only, called during init
//   - resolve()/count(): read-only after init, safe for concurrent access
//
// Usage:
//   Entity plugin wraps this for IntelligenceFlagRegistry (adds per-type
//   defaults). Any system needing runtime-extensible named flags can use this
//   directly.
// ============================================================================

/// Bitmask of named flags. Each bit corresponds to a registered flag name.
using FlagSet = uint64_t;

inline constexpr FlagSet NO_FLAGS = 0;
inline constexpr FlagSet SINGLE_FLAG = 1ULL;
inline constexpr uint8_t MAX_FLAGS = 64;

/// Check whether a specific flag bit is set.
inline bool hasFlag(FlagSet flags, uint8_t bit_index) {
  return (flags & (SINGLE_FLAG << bit_index)) != 0;
}

/// Set a specific flag bit.
inline FlagSet setFlag(FlagSet flags, uint8_t bit_index) {
  return flags | (SINGLE_FLAG << bit_index);
}

/// Clear a specific flag bit.
inline FlagSet clearFlag(FlagSet flags, uint8_t bit_index) {
  return flags & ~(SINGLE_FLAG << bit_index);
}

/// Generic registry mapping string names to bit indices in a FlagSet.
/// Plugins claim named flags at init; systems resolve names at load time.
// Thread-safe: populated at init (single-threaded); read-only after.
class NamedFlagRegistry {
public:
  /// Claim a named flag. Returns bit index (0-63).
  /// Returns nullopt if name already claimed or capacity exhausted.
  std::optional<uint8_t> claim(std::string_view name);

  /// Resolve a flag name to its bit index.
  /// Returns nullopt if not registered.
  [[nodiscard]] std::optional<uint8_t> resolve(std::string_view name) const;

  /// Number of claimed flags.
  [[nodiscard]] uint8_t count() const;

private:
  struct FlagEntry {
    /// Human-readable flag name used for registration and lookup.
    std::string name;
    /// Bit position (0-63) assigned to this flag in the FlagSet bitmask.
    uint8_t bit_index = 0;
  };

  /// All registered flag entries, in order of registration.
  std::vector<FlagEntry> entries_;
  /// Next bit index to assign when a new flag is claimed.
  uint8_t next_bit_ = 0;
};

}  // namespace eng

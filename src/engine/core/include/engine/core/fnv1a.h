#pragma once

#include <cstdint>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Constexpr 64-bit FNV-1a hash function.
//
// Used to generate deterministic event IDs from event name strings at
// compile time. Also callable at runtime for dynamic event registration.
//
// Thread Safety: Pure function, no state.
// ============================================================================

/// FNV-1a 64-bit offset basis.
constexpr uint64_t FNV1A_OFFSET = 14695981039346656037ULL;

/// FNV-1a 64-bit prime multiplier.
constexpr uint64_t FNV1A_PRIME = 1099511628211ULL;

/// Compute a 64-bit FNV-1a hash of the given string.
constexpr uint64_t fnv1a(std::string_view str) noexcept {
  auto hash = FNV1A_OFFSET;
  for (const auto c : str) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hash *= FNV1A_PRIME;
  }
  return hash;
}

}  // namespace eng

#pragma once

/// @file utf8-step.h
/// @brief Decoding UTF-8 one character at a time.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace eng {

/// One character decoded from UTF-8.
struct Utf8Step {
  /// Its codepoint; U+FFFD for a malformed sequence.
  uint32_t codepoint = 0;
  /// Bytes it took, at least one, so a walk always moves on.
  std::size_t length = 1;
};

/// The character of @p text starting at byte @p at.
[[nodiscard]] Utf8Step decodeUtf8(std::string_view text, std::size_t at);

}  // namespace eng

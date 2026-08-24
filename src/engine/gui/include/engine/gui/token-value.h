#pragma once

#include <cstdint>
#include <string>

namespace eng {

/// @thread_safety Main thread only.
enum class TokenType : uint8_t {
  COLOR,
  FLOAT,
  STRING,
  INT,
};

/// Packed RGBA magenta fallback for missing theme tokens.
constexpr uint32_t FALLBACK_TOKEN_COLOR = 0xFF00FFFF;

/// @thread_safety Main thread only.
struct TokenValue {
  /// Data type of this token.
  TokenType type = TokenType::COLOR;
  /// Packed RGBA colour value (magenta fallback for missing tokens).
  uint32_t color = FALLBACK_TOKEN_COLOR;
  /// Numeric float value.
  float number = 0.0f;
  /// Numeric integer value.
  int32_t integer = 0;
  /// String value.
  std::string str{};
};

}  // namespace eng

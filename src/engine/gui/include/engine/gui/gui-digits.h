#pragma once

/// @file gui-digits.h
/// @brief How wide digits are set.
/// @par Threading
/// Constants only.

#include <cstdint>

namespace eng {

/// Whether each digit takes its own width or all take the widest's.
enum class GuiDigits : uint8_t {
  /// Each digit its own width: body text.
  PROPORTIONAL,
  /// Every digit as wide as the widest, centred in it, so a changing
  /// number does not jitter: timers, ammo, scores. CSS's
  /// `font-variant-numeric: tabular-nums`.
  TABULAR,
};

}  // namespace eng

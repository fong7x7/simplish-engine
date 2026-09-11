#pragma once

/// @file sin-cos.h
/// @brief Sine and cosine that give the same bits on every machine.
/// @par Threading
/// Pure functions over value types.

namespace eng::math {

/// The sine and cosine of one angle, computed together.
struct SinCos {
  /// The sine of the angle.
  float sin = 0.0F;
  /// The cosine of the angle.
  float cos = 1.0F;
};

/// The sine and cosine of @p degrees, the same to the bit on every target.
///
/// The deterministic replacement ADR-002 and Engine REQUIREMENTS §3 promise
/// simulation code in place of libm's `sin` and `cos`, whose last bit is up
/// to each platform's library. It uses only operations IEEE-754 rounds
/// exactly — `fmod`, division, `nearbyint`, and multiply-add in double
/// precision with FMA contraction off — so two machines that agree on the
/// input agree on the output.
///
/// The angle is taken in degrees because everything authored is: a field
/// of view, a turn rate, a prop's rotation. Reducing whole quadrants in
/// degrees is exact, so 90° gives exactly 1 and 0, not something a rounding
/// of π away from them. Accurate to within one float ulp of the true value.
[[nodiscard]] SinCos sinCosDegrees(float degrees);

}  // namespace eng::math

#pragma once

/// @file turn-toward.h
/// @brief Turning a heading toward another by at most a fixed angle.
/// @par Threading
/// Pure functions over value types.

#include <engine/math/sin-cos.h>
#include <engine/math/vec2.h>

namespace eng::math {

/// @p v turned counterclockwise by the angle whose sine and cosine are
/// @p turn — clockwise when the sine is negative.
[[nodiscard]] Vec2 rotateBy(Vec2 v, SinCos turn);

/// The unit heading @p facing turned toward @p desired by no more than the
/// angle whose sine and cosine are @p max_turn.
///
/// @p desired need not be unit length; one of zero length leaves @p facing
/// as it is. A heading already within @p max_turn of @p desired lands on it
/// exactly. Otherwise it turns by the whole angle, the short way round —
/// counterclockwise when @p desired is directly behind, so a reversal has
/// one answer on every machine. The result is renormalised, so a heading
/// turned every tick for an hour is still unit length.
///
/// No angles and no trigonometry: a dot product, a cross product and a
/// square root, all exact under IEEE-754 (ADR-002). @p max_turn is worked
/// out once, from a turn rate, with `sinCosDegrees`.
[[nodiscard]] Vec2 turnToward(Vec2 facing, Vec2 desired, SinCos max_turn);

}  // namespace eng::math

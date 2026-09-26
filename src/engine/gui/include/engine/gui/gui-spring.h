#pragma once

/// @file gui-spring.h
/// @brief A damped spring, as a timing curve.
/// @par Threading
/// A value type; pure.

namespace eng {

/// A unit mass on a spring pulled from 0 to rest at 1: stiff springs move
/// quickly, light damping bounces. `at(seconds)` is where it is; it
/// settles — within a thousandth — after `settleSeconds()`.
struct GuiSpring {
  /// How hard it pulls towards rest, per unit of distance.
  float stiffness = 170.0f;
  /// How hard it resists moving, per unit of speed.
  float damping = 18.0f;

  /// Where it is @p seconds after letting go: 0 at the start, 1 at rest,
  /// past 1 while it overshoots.
  [[nodiscard]] float at(float seconds) const;

  /// How long it takes to come within a thousandth of rest, and stay.
  [[nodiscard]] float settleSeconds() const;

  /// Its position at the fraction @p t of `settleSeconds`, exactly 1 at
  /// @p t = 1: the spring as an easing over any duration.
  [[nodiscard]] float eased(float t) const;
};

/// The spring `GuiEasing::SPRING` uses: quick, with a little bounce.
inline constexpr GuiSpring GUI_SPRING_DEFAULT{};

}  // namespace eng

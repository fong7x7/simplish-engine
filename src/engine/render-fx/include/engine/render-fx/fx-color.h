#pragma once

/// @file fx-color.h
/// @brief The colour of a particle, as the effects pass blends it.
/// @par Threading
/// A value type.

namespace eng {

/// Premultiplied linear RGBA: what a particle adds to the pixel under it,
/// and how much of that pixel it hides.
///
/// The effects pass blends `source + destination * (1 - alpha)`, so one
/// colour covers both kinds of particle a burst needs: a glow is alpha
/// zero and only ever adds light, whatever order it is drawn in, and smoke
/// is dark with alpha near one and hides what is behind it. Anything
/// between is a glow that also dims its background, like a cloud of dust
/// lit from inside.
struct FxColor {
  /// Red light added, linear.
  float r = 0.0f;
  /// Green light added, linear.
  float g = 0.0f;
  /// Blue light added, linear.
  float b = 0.0f;
  /// How much of what is behind it this hides, 0 to 1.
  float a = 0.0f;

  /// Two colours are equal when every component is, to the last bit.
  bool operator==(const FxColor&) const = default;
};

/// @p from, @p t of the way to @p to, component by component.
[[nodiscard]] FxColor mixFxColor(const FxColor& from, const FxColor& to,
                                 float t);

}  // namespace eng

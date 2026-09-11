#pragma once

/// @file editor-scale-slider.h
/// @brief Where a scale sits on the properties panel's slider, and back.
/// @par Threading Thread-safe (pure functions over value types).

namespace eng::editor {

/// How many stops the step buttons divide one doubling into.
///
/// A quarter of an octave is about a fifth bigger or smaller per click:
/// enough that a few clicks visibly resize a prop, few enough that halving
/// or doubling one is four.
inline constexpr float EDITOR_SCALE_STOPS_PER_DOUBLING = 4.0f;

/// Where @p scale sits along the slider, from 0 at `EDITOR_SCALE_MIN` to 1
/// at `EDITOR_SCALE_MAX`.
///
/// Logarithmic rather than linear, because scale is felt as a ratio: on a
/// linear track from an eighth to eight, the whole range below 1 would be
/// crammed into the first eighth of it. On this one, halving and doubling
/// are the same distance either side of 1, and 1 is the exact middle.
[[nodiscard]] float editorScaleSliderFraction(float scale);

/// The scale at @p fraction along the slider — the inverse of
/// `editorScaleSliderFraction`. A fraction past either end is the scale at
/// that end.
[[nodiscard]] float editorScaleFromSliderFraction(float fraction);

/// The scale @p steps presses of a step button move @p scale to: that many
/// stops up for a positive count, down for a negative one.
///
/// Stops are fixed points — every quarter of a doubling from 1 — rather than
/// a fixed ratio from wherever the value is. So a prop dragged to 1.03 and
/// stepped down lands on exactly 1, which a slider on its own can almost
/// never be let go on.
[[nodiscard]] float editorScaleStepped(float scale, int steps);

}  // namespace eng::editor

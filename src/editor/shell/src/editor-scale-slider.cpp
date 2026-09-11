#include <algorithm>
#include <cmath>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-scale-slider.h>

namespace eng::editor {

namespace {

  /// How close to a stop a value has to be to count as on it. A value that
  /// is on one — 1, or a value a step button produced — would otherwise sit
  /// a rounding error either side of it, and a step from it could land on
  /// the stop it is already at.
  constexpr float ON_STOP = 1e-3f;

  /// The slider's two ends, as doublings from 1.
  float lowestDoubling() {
    return std::log2(EDITOR_SCALE_MIN);
  }
  float highestDoubling() {
    return std::log2(EDITOR_SCALE_MAX);
  }

  float clampScale(float scale) {
    return std::clamp(scale, EDITOR_SCALE_MIN, EDITOR_SCALE_MAX);
  }

}  // namespace

float editorScaleSliderFraction(float scale) {
  const float doublings = std::log2(clampScale(scale));
  return (doublings - lowestDoubling()) /
         (highestDoubling() - lowestDoubling());
}

float editorScaleFromSliderFraction(float fraction) {
  const float along = std::clamp(fraction, 0.0f, 1.0f);
  const float doublings =
      lowestDoubling() + along * (highestDoubling() - lowestDoubling());
  return clampScale(std::exp2(doublings));
}

float editorScaleStepped(float scale, int steps) {
  if (steps == 0) {
    return clampScale(scale);
  }
  // Which stop the value is at, or between: 0 is 1, 4 is 2, -4 is a half.
  const float stop =
      std::log2(clampScale(scale)) * EDITOR_SCALE_STOPS_PER_DOUBLING;
  // From the stop at or below for a step up, at or above for a step down,
  // so the first step from between two stops lands on the nearer one in
  // that direction.
  const float from =
      steps > 0 ? std::floor(stop + ON_STOP) : std::ceil(stop - ON_STOP);
  const float next = from + static_cast<float>(steps);
  return clampScale(std::exp2(next / EDITOR_SCALE_STOPS_PER_DOUBLING));
}

}  // namespace eng::editor

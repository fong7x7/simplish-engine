#include "engine/gui/gui-color.h"

#include <algorithm>
#include <cmath>

namespace eng {

namespace {

  /// Below this the sRGB transfer function is a straight line.
  constexpr float SRGB_LINEAR_CUTOFF = 0.04045F;
  /// Slope of that straight segment.
  constexpr float SRGB_LINEAR_SLOPE = 12.92F;
  /// Offset and scale of the curved segment.
  constexpr float SRGB_OFFSET = 0.055F;
  constexpr float SRGB_SCALE = 1.055F;
  /// Exponent of the curved segment.
  constexpr float SRGB_GAMMA = 2.4F;
  /// Byte to unit range.
  constexpr float BYTE_TO_UNIT = 1.0F / 255.0F;

}  // namespace

float srgbByteToLinear(uint8_t channel) {
  const float value = static_cast<float>(channel) * BYTE_TO_UNIT;
  if (value <= SRGB_LINEAR_CUTOFF) {
    return value / SRGB_LINEAR_SLOPE;
  }
  return std::pow((value + SRGB_OFFSET) / SRGB_SCALE, SRGB_GAMMA);
}


GuiColor GuiColor::lerp(const GuiColor& a, const GuiColor& b, float t) {
  float ct = std::clamp(t, 0.0f, 1.0f);
  float inv = 1.0f - ct;
  return {static_cast<uint8_t>(static_cast<float>(a.r) * inv +
                               static_cast<float>(b.r) * ct),
          static_cast<uint8_t>(static_cast<float>(a.g) * inv +
                               static_cast<float>(b.g) * ct),
          static_cast<uint8_t>(static_cast<float>(a.b) * inv +
                               static_cast<float>(b.b) * ct),
          static_cast<uint8_t>(static_cast<float>(a.a) * inv +
                               static_cast<float>(b.a) * ct)};
}

GuiColor GuiColor::applyOpacity(const GuiColor& c, float opacity) {
  float clamped = std::clamp(opacity, 0.0f, 1.0f);
  auto new_a = static_cast<uint8_t>(static_cast<float>(c.a) * clamped);
  return {c.r, c.g, c.b, new_a};
}

}  // namespace eng

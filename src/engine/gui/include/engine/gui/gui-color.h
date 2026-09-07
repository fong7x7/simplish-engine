#pragma once

/// @file gui-color.h
/// @brief RGBA color type for GUI draw calls with opacity and lerp helpers.
/// @threading Main-thread only.

#include <cstdint>

namespace eng {

/// RGBA color for UI draw calls (0-255 per channel).
/// @threading Main-thread only.
struct GuiColor {
  /// Red channel.
  uint8_t r = 0;
  /// Green channel.
  uint8_t g = 0;
  /// Blue channel.
  uint8_t b = 0;
  /// Alpha channel (255 = fully opaque).
  uint8_t a = 255;

  /// Pack RGBA into a single uint32_t.
  inline uint32_t pack() const {
    return (static_cast<uint32_t>(r)) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
  }

  /// Linearly interpolate between two colors channel-by-channel.
  static GuiColor lerp(const GuiColor& a, const GuiColor& b, float t);

  /// Multiply alpha channel by an opacity factor [0, 1].
  static GuiColor applyOpacity(const GuiColor& c, float opacity);
};

/// Convert one sRGB-encoded colour byte to a linear value in [0, 1].
///
/// UI colours are authored as sRGB bytes, which is what a colour picker
/// shows and what a shader reading a byte must decode. A render pass clear
/// is the one place that does not decode: an sRGB render target encodes on
/// write, so its clear value is linear, and handing it a raw byte over 255
/// paints the surface visibly lighter than the colour asked for.
[[nodiscard]] float srgbByteToLinear(uint8_t channel);

inline constexpr GuiColor GUI_COLOR_WHITE{255, 255, 255, 255};
inline constexpr GuiColor GUI_COLOR_BLACK{0, 0, 0, 255};

}  // namespace eng

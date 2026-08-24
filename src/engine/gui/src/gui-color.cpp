#include "engine/gui/gui-color.h"

#include <algorithm>

namespace eng {

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

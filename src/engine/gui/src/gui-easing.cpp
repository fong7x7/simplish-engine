#include "engine/gui/gui-easing.h"

#include <engine/gui/gui-cubic-bezier.h>
#include <engine/gui/gui-spring.h>
#include <engine/math/math.h>
#include <optional>

namespace eng {

namespace {

  /// The curves defined by a shape rather than a formula; nothing for the
  /// rest.
  std::optional<float> curveEasing(GuiEasing easing, float t) {
    switch (easing) {
      case GuiEasing::EASE:
        return GUI_BEZIER_EASE.at(t);
      case GuiEasing::EMPHASIZED:
        return GUI_BEZIER_EMPHASIZED.at(t);
      case GuiEasing::BACK_OUT:
        return GUI_BEZIER_BACK_OUT.at(t);
      case GuiEasing::SPRING:
        return GUI_SPRING_DEFAULT.eased(t);
      default:
        return std::nullopt;
    }
  }

}  // namespace

float applyEasing(GuiEasing easing, float t) {
  float ct = eng::math::clamp(t, 0.0f, 1.0f);
  if (const auto curved = curveEasing(easing, ct)) {
    return *curved;
  }
  switch (easing) {
    case GuiEasing::LINEAR:
      return ct;
    case GuiEasing::EASE_IN:
      return ct * ct * ct;
    case GuiEasing::EASE_OUT: {
      float inv = 1.0f - ct;
      return 1.0f - inv * inv * inv;
    }
    case GuiEasing::EASE_IN_OUT:
      return eng::math::smoothstep(ct);
    default:
      return ct;
  }
  return ct;
}

}  // namespace eng

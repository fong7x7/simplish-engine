#include "engine/gui/gui-easing.h"

#include <engine/math/math.h>

namespace eng {

float applyEasing(GuiEasing easing, float t) {
  float ct = eng::math::clamp(t, 0.0f, 1.0f);
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
  }
  return ct;
}

}  // namespace eng

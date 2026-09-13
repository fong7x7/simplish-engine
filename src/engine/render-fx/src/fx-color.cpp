#include <engine/render-fx/fx-color.h>

namespace eng {

FxColor mixFxColor(const FxColor& from, const FxColor& to, float t) {
  return {from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t,
          from.b + (to.b - from.b) * t, from.a + (to.a - from.a) * t};
}

}  // namespace eng

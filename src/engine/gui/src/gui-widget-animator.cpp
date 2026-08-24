#include "engine/gui/gui-widget-animator.h"

#include "engine/gui/gui-easing.h"
#include "engine/gui/gui-panel.h"
#include "engine/gui/gui-widget.h"

#include <algorithm>

namespace eng {

/// Interpolate a scalar value between start and target at eased time.
static float lerpScalar(float a, float b, float t) {
  return a + (b - a) * t;
}

/// Returns true if the property targets a color (vs scalar).
static bool isColorProperty(GuiAnimProperty prop) {
  return prop == GuiAnimProperty::FILL_COLOR ||
         prop == GuiAnimProperty::BORDER_COLOR;
}

/// Returns a pointer to the rect field for the given property, or nullptr.
static float* rectScalarTarget(Rect& rect, GuiAnimProperty prop) {
  switch (prop) {
    case GuiAnimProperty::RECT_X:
      return &rect.x;
    case GuiAnimProperty::RECT_Y:
      return &rect.y;
    case GuiAnimProperty::RECT_W:
      return &rect.w;
    case GuiAnimProperty::RECT_H:
      return &rect.h;
    default:
      return nullptr;
  }
}

void GuiWidgetAnimator::applyScalar(GuiWidget& w, GuiAnimProperty prop,
                                    float val) {
  float* target = rectScalarTarget(w.rect, prop);
  if (target != nullptr) {
    *target = val;
    return;
  }
  if (prop == GuiAnimProperty::OPACITY) {
    w.opacity = val;
  }
}

void GuiWidgetAnimator::applyColor(GuiWidget& w, GuiAnimProperty prop,
                                   const GuiColor& val) {
  auto* panel = dynamic_cast<GuiPanel*>(&w);
  if (panel == nullptr) {
    return;
  }
  switch (prop) {
    case GuiAnimProperty::FILL_COLOR:
      panel->fill_color = val;
      return;
    case GuiAnimProperty::BORDER_COLOR:
      panel->border_color = val;
      return;
    default:
      return;
  }
}

void GuiWidgetAnimator::tickOne(GuiAnimation& anim, GuiWidget& widget,
                                float dt) {
  anim.elapsed += dt;
  float t = (anim.duration > 0.0f)
                ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f)
                : 1.0f;
  float eased = applyEasing(anim.easing, t);
  if (isColorProperty(anim.property)) {
    applyColor(widget, anim.property,
               GuiColor::lerp(anim.start.color, anim.target.color, eased));
  } else {
    applyScalar(widget, anim.property,
                lerpScalar(anim.start.scalar, anim.target.scalar, eased));
  }
}

bool GuiWidgetAnimator::tick(GuiWidget& widget, float dt) {
  for (auto& anim : animations) {
    tickOne(anim, widget, dt);
  }
  // ranges::stable_partition subrange incompatible with downstream erase
  // NOLINTNEXTLINE(modernize-use-ranges)
  auto it = std::stable_partition(
      animations.begin(), animations.end(),
      [](const GuiAnimation& a) { return a.elapsed < a.duration; });
  for (auto done = it; done != animations.end(); ++done) {
    if (done->on_complete) {
      done->on_complete();
    }
  }
  animations.erase(it, animations.end());
  return !animations.empty();
}

void GuiWidgetAnimator::add(const GuiAnimation& anim) {
  auto it = std::ranges::find_if(animations, [&](const GuiAnimation& a) {
    return a.property == anim.property;
  });
  if (it != animations.end()) {
    *it = anim;
  } else {
    animations.push_back(anim);
  }
}

void GuiWidgetAnimator::cancelAll() {
  animations.clear();
}

bool GuiWidgetAnimator::hasActiveAnimations() const {
  return !animations.empty();
}

}  // namespace eng

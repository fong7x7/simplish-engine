#include <algorithm>
#include <engine/gui/gui-easing.h>
#include <engine/gui/gui-style-transition.h>

namespace eng {

namespace {

  /// Seconds short of a blend's end that count as the end.
  constexpr float TRANSITION_EPSILON = 1.0e-5f;

  /// Whether @p a and @p b would draw alike.
  bool sameColor(const GuiColor& a, const GuiColor& b) {
    return a.pack() == b.pack();
  }

  bool sameStyle(const GuiStateStyle& a, const GuiStateStyle& b) {
    return sameColor(a.fill, b.fill) && sameColor(a.text, b.text) &&
           sameColor(a.border, b.border) && a.border_width == b.border_width &&
           a.radius == b.radius && a.elevation == b.elevation;
  }

}  // namespace

void GuiStyleTransition::retarget(const GuiStateStyle& target, float seconds) {
  if (!started_ || seconds <= 0.0f) {
    from_ = to_ = current_ = target;
    started_ = true;
    elapsed_ = duration_ = 0.0f;
    return;
  }
  if (sameStyle(target, to_)) {
    return;
  }
  from_ = current_;
  to_ = target;
  duration_ = seconds;
  elapsed_ = 0.0f;
}

void GuiStyleTransition::tick(float dt) {
  if (!isBlending()) {
    return;
  }
  elapsed_ += dt;
  // Within a rounding error of the end is the end: 0.08 + 0.02 is not
  // quite 0.1 in floats, and a blend must not hang a hair short of done.
  if (duration_ - elapsed_ < TRANSITION_EPSILON) {
    elapsed_ = duration_;
  }
  const float t = applyEasing(GuiEasing::EASE_OUT, elapsed_ / duration_);
  current_ = GuiStateStyle::lerp(from_, to_, t);
}

const GuiStateStyle& GuiStyleTransition::current() const {
  return current_;
}

bool GuiStyleTransition::isBlending() const {
  return started_ && elapsed_ < duration_;
}

}  // namespace eng

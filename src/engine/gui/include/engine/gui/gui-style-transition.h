#pragma once

/// @file gui-style-transition.h
/// @brief Blends a widget's look from one state's style to the next.
/// @par Threading
/// Main thread only.

#include "gui-state-style.h"

namespace eng {

/// Eases a widget's drawn style towards the style of the state it is in,
/// so a hover fades in over the theme's transition time instead of
/// snapping. The first target is taken at once.
class GuiStyleTransition {
public:
  /// Head for @p target, taking @p seconds to get there. Retargeting to
  /// the target it already has does nothing; retargeting mid-blend starts
  /// from where it has got to.
  void retarget(const GuiStateStyle& target, float seconds);

  /// Move the blend on by @p dt seconds.
  void tick(float dt);

  /// The style to draw now.
  [[nodiscard]] const GuiStateStyle& current() const;

  /// Whether a blend is under way.
  [[nodiscard]] bool isBlending() const;

private:
  /// Where the blend started.
  GuiStateStyle from_{};
  /// Where it is heading.
  GuiStateStyle to_{};
  /// What to draw now.
  GuiStateStyle current_{};
  /// Seconds the blend takes.
  float duration_ = 0.0f;
  /// Seconds it has run.
  float elapsed_ = 0.0f;
  /// Whether any target has been set yet.
  bool started_ = false;
};

}  // namespace eng

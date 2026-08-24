#pragma once

/// @file gui-widget-animator-ops.h
/// @brief Bridge functions that apply GuiWidgetAnimator state to GuiWidget.
/// Exists to break the header cycle between gui-widget.h and
/// gui-widget-animator.h: the animator stores animation data (no widget dep),
/// and these operations apply interpolated values to a widget.
/// @threading Main-thread only.

#include "gui-animation.h"
#include "gui-color.h"
#include "gui-widget-animator.h"
#include "gui-widget.h"

namespace eng {

/// Tick all animations by dt seconds. Applies interpolated values to
/// the widget. Returns true if any animations are still active.
/// @threading Main-thread only.
bool tickAnimator(GuiWidgetAnimator& animator, GuiWidget& widget, float dt);

/// Apply an interpolated scalar to the appropriate widget field.
/// @threading Main-thread only.
void applyAnimScalar(GuiWidget& widget, GuiAnimProperty prop, float val);

/// Apply an interpolated color to the appropriate panel field.
/// @threading Main-thread only.
void applyAnimColor(GuiWidget& widget, GuiAnimProperty prop,
                    const GuiColor& val);

/// Advance one animation and apply its value to the widget.
/// @threading Main-thread only.
void tickOneAnimation(GuiAnimation& anim, GuiWidget& widget, float dt);

}  // namespace eng

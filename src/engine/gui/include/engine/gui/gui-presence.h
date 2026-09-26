#pragma once

/// @file gui-presence.h
/// @brief How a widget arrives and goes: faded, scaled and moved from, or
/// to, where it rests.
/// @par Threading
/// A value type.

#include "gui-easing.h"

namespace eng {

/// Where a widget is drawn as it starts to enter, or as it finishes
/// leaving — CSS's `@starting-style` and exit transition in one. At rest a
/// widget is fully opaque, at scale 1, not offset; `GuiWidget::enter`
/// animates from this to rest, `GuiWidget::leave` from rest to this.
/// Drawing only: layout and hit testing see the widget at rest.
struct GuiPresence {
  /// Its opacity there.
  float opacity = 0.0f;
  /// Its scale there, about its centre.
  float scale = 1.0f;
  /// How far right of rest it is there, in layout pixels.
  float offset_x = 0.0f;
  /// How far below rest it is there.
  float offset_y = 0.0f;
  /// How long the move takes.
  float seconds = 0.16f;
  /// Its curve.
  GuiEasing easing = GuiEasing::EMPHASIZED;

  /// The same place, for leaving: quicker, and accelerating away rather
  /// than settling — exits should not linger.
  [[nodiscard]] constexpr GuiPresence exiting() const {
    return {opacity,  scale,          offset_x,
            offset_y, seconds * 0.7f, GuiEasing::EASE_IN};
  }
};

/// Fade only.
inline constexpr GuiPresence GUI_PRESENCE_FADE{.easing = GuiEasing::EASE_OUT};
/// Fade up from slightly small: a dialog.
inline constexpr GuiPresence GUI_PRESENCE_POP{.scale = 0.96f, .seconds = 0.18f};
/// Fade down from just above: a menu dropping from its title.
inline constexpr GuiPresence GUI_PRESENCE_DROP{.offset_y = -6.0f,
                                               .seconds = 0.14f};
/// Fade up from just below: a toast, a sheet.
inline constexpr GuiPresence GUI_PRESENCE_RISE{.offset_y = 10.0f,
                                               .seconds = 0.2f};

}  // namespace eng

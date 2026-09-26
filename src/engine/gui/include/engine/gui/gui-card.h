#pragma once

/// @file gui-card.h
/// @brief A raised surface in the theme's card look.
/// @par Threading
/// Main thread only.

#include "gui-elevation.h"
#include "gui-panel.h"

#include <optional>

namespace eng {

/// A container drawn in the theme's `card` look — raised on a shadow,
/// rounded, bordered, lifting under the pointer and taking the accent
/// border when `selected`. The box for a dialog, a tile in a grid, a group
/// of settings.
/// @thread_safety Main thread only.
class GuiCard : public GuiPanel {
public:
  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// The theme's card look.
  [[nodiscard]] const GuiStateStyles*
  themeStyles(const GuiTheme& theme) const override;

  /// Its box, raised to `elevation` when that is set.
  void render(const GuiDrawContext& ctx) const override;

  /// How high it is raised, in place of the theme's for its state: HIGH
  /// for a dialog, NONE for a flat inset group.
  std::optional<GuiElevation> elevation{};
};

}  // namespace eng

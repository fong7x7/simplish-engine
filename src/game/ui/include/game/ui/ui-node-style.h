#pragma once

/// @file ui-node-style.h
/// @brief How one node of a game screen is laid out and coloured.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/gui/gui-button-variant.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-elevation.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-text-role.h>
#include <engine/gui/gui-text-wrap.h>
#include <engine/gui/layout-auto-margins.h>
#include <engine/gui/layout-edges.h>
#include <engine/gui/layout-engine.h>
#include <optional>

namespace eng::game {

/// The part of the GUI's `LayoutStyle` a screen file sets, its colours,
/// and its look in the screens' theme. Sizes are pixels, border-box; `-1`
/// is automatic, as in `LayoutStyle`.
struct UiNodeStyle {
  /// How a panel lays its children out: in a column, or a row.
  FlexDirection direction = FlexDirection::COLUMN;
  /// Space between a panel's children.
  float gap = 0.0F;
  /// Space inside the node's edge.
  Edges padding{};
  /// Space outside it.
  Edges margin{};
  /// The sides whose margin is `auto`: they take the free space, pushing
  /// the node over or centring it.
  LayoutAutoMargins margin_auto{};
  /// Its width; -1 sizes it to its content.
  float width = -1.0F;
  /// Its height; -1 sizes it to its content.
  float height = -1.0F;
  /// The narrowest it may be.
  float min_width = 0.0F;
  /// The shortest it may be.
  float min_height = 0.0F;
  /// The widest it may be; -1 for no limit.
  float max_width = -1.0F;
  /// The tallest it may be; -1 for no limit.
  float max_height = -1.0F;
  /// Its width as a percentage of its parent's content box; -1 for none.
  float width_percent = -1.0F;
  /// Its height as a percentage of its parent's content box; -1 for none.
  float height_percent = -1.0F;
  /// Its share of the space left on its line; unset is 0, or 1 for a
  /// spacer.
  std::optional<float> grow{};
  /// How much of an overflow it gives up; unset is 1.
  std::optional<float> shrink{};
  /// How a panel places its children across its axis.
  Align align_items = Align::STRETCH;
  /// How a panel places its children along its axis.
  Align justify = Align::START;
  /// How this node sits across its parent's axis, overriding the parent.
  Align align_self = Align::AUTO;
  /// Its background: a panel's, a button's, a bar's filled part. Unset is
  /// the node's own default.
  std::optional<GuiColor> fill{};
  /// Its text's colour, or a bar's empty part.
  std::optional<GuiColor> color{};
  /// How round its corners are.
  float radius = 0.0F;
  /// A panel's border width; 0 for none.
  float border = 0.0F;
  /// A panel's border colour; unset is the theme's `border`.
  std::optional<GuiColor> border_color{};
  /// How raised a panel is: the theme's shadow for that step.
  GuiElevation elevation = GuiElevation::NONE;
  /// How opaque it and everything in it are, 0 to 1.
  float opacity = 1.0F;
  /// A button's role: its look in the theme, unless `fill` gives one.
  GuiButtonVariant variant = GuiButtonVariant::NEUTRAL;
  /// Its text's role — the theme's size and weight for it; unset is a
  /// label's `BODY`, a button's `LABEL`.
  std::optional<GuiTextRole> role{};
  /// Its text's size in pixels, over the role's; 0 for the role's.
  float text_size = 0.0F;
  /// Its text's weight, 100 to 900, over the role's; 0 for the role's.
  uint16_t weight = 0;
  /// Whether a label's text wraps at its width.
  GuiTextWrap wrap = GuiTextWrap::NONE;
  /// Where a label's text sits in its box.
  GuiLabelAlign text_align = GuiLabelAlign::LEFT;
};

}  // namespace eng::game

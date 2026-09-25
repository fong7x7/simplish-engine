#pragma once

/// @file ui-node-style.h
/// @brief How one node of a game screen is laid out and coloured.
/// @par Threading
/// A value type.

#include <engine/gui/gui-color.h>
#include <engine/gui/layout-edges.h>
#include <engine/gui/layout-engine.h>
#include <optional>

namespace eng::game {

/// The part of the GUI's `LayoutStyle` a screen file sets, and its colours.
/// Sizes are pixels, border-box; `-1` is automatic, as in `LayoutStyle`.
struct UiNodeStyle {
  /// How a panel lays its children out: in a column, or a row.
  FlexDirection direction = FlexDirection::COLUMN;
  /// Space between a panel's children.
  float gap = 0.0F;
  /// Space inside the node's edge.
  Edges padding{};
  /// Space outside it.
  Edges margin{};
  /// Its width; -1 sizes it to its content.
  float width = -1.0F;
  /// Its height; -1 sizes it to its content.
  float height = -1.0F;
  /// The narrowest it may be.
  float min_width = 0.0F;
  /// The shortest it may be.
  float min_height = 0.0F;
  /// Its share of the space left on its line; unset is 0, or 1 for a
  /// spacer.
  std::optional<float> grow{};
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
};

}  // namespace eng::game

#pragma once

/// @file flex-layout.h
/// @brief The flexbox measure and arrange steps `GuiWidgetTree::computeLayout`
/// and `GuiWidget::arrangeChildren` run.
/// @par Threading Main thread only.

#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/gui-widget.h>
#include <engine/gui/layout-size.h>

namespace eng {

/// @p widget's border-box size: its explicit size on each axis that has
/// one, otherwise the larger of its own content and its in-flow children's
/// plus its padding — all within its min and max. Reads the children's
/// `tree_measured`, so they are measured first.
[[nodiscard]] LayoutSize measureBorderBox(const GuiWidgetTree& tree,
                                          const GuiWidget& widget,
                                          const GuiDrawContext& ctx);

/// Place @p parent's children inside @p box, its border box, by flexbox:
/// in-flow children along its direction within its padding, spaced by
/// their margins and its gap, grown, shrunk, wrapped and aligned; absolute
/// ones at their offsets. Each is arranged in turn, so the whole subtree
/// is laid out.
void arrangeFlexChildren(GuiWidgetTree& tree, const GuiWidget& parent,
                         const Rect& box);

}  // namespace eng

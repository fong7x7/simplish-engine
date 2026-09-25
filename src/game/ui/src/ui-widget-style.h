#pragma once

/// @file ui-widget-style.h
/// @brief How a game screen's nodes become styled GUI widgets.
/// @par Threading
/// Pure, over the widget it is given.

#include <engine/gui/gui-widget.h>
#include <engine/gui/layout-engine.h>
#include <game/ui/ui-anchor.h>
#include <game/ui/ui-node.h>

namespace eng::game {

/// A label's text colour, unless the node gives one.
inline constexpr GuiColor UI_TEXT_COLOR{236, 236, 242, 255};
/// A button's face, unless the node gives one.
inline constexpr GuiColor UI_BUTTON_FILL{52, 58, 74, 240};
/// A button's face under the pointer or the focus.
inline constexpr GuiColor UI_BUTTON_HOVER{82, 94, 122, 255};
/// A bar's filled part, unless the node gives one.
inline constexpr GuiColor UI_BAR_FILL{96, 200, 112, 255};
/// A bar's empty part, unless the node gives one.
inline constexpr GuiColor UI_BAR_EMPTY{36, 38, 46, 230};
/// A bar's height, unless the node gives one.
inline constexpr float UI_BAR_HEIGHT = 10.0F;
/// What a menu dims the game behind it with.
inline constexpr GuiColor UI_MENU_SCRIM{0, 0, 0, 150};
/// Nothing: a panel with no fill.
inline constexpr GuiColor UI_CLEAR{0, 0, 0, 0};

/// Set @p widget's layout from @p node's style — and a spacer's grow of 1
/// when it gives none — and have it draw with its own style, never the
/// editor's theme.
void styleUiWidget(GuiWidget& widget, const UiNode& node);

/// Set @p layout, the covering panel's, to place a screen's root by
/// @p anchor, @p inset from the edges.
void anchorUiRoot(LayoutStyle& layout, UiAnchor anchor, float inset);

}  // namespace eng::game

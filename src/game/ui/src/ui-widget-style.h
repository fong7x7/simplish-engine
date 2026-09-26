#pragma once

/// @file ui-widget-style.h
/// @brief How a game screen's nodes become styled GUI widgets.
/// @par Threading
/// Pure, over the widget it is given.

#include <engine/gui/gui-theme.h>
#include <engine/gui/gui-widget.h>
#include <engine/gui/layout-engine.h>
#include <game/ui/ui-anchor.h>
#include <game/ui/ui-node.h>

namespace eng::game {

/// A button's corner radius, unless the node gives one.
inline constexpr float UI_BUTTON_RADIUS = 6.0F;
/// A button's padding round its text, unless the node gives some.
inline constexpr Edges UI_BUTTON_PADDING{8.0F, 18.0F, 8.0F, 18.0F};
/// A bar's height, unless the node gives one.
inline constexpr float UI_BAR_HEIGHT = 10.0F;
/// How long a menu's nodes take to glide to a new place.
inline constexpr float UI_MENU_GLIDE_SECONDS = 0.18F;
/// Nothing: a panel with no fill.
inline constexpr GuiColor UI_CLEAR{0, 0, 0, 0};

/// Set @p widget's layout and opacity from @p node's style — and a
/// spacer's grow of 1 when it gives none. Colours and looks are set per
/// widget kind, by `UiScreenView`, from the node or the screens' theme.
void styleUiWidget(GuiWidget& widget, const UiNode& node);

/// A button's look in every state from @p style's fill, text colour and
/// radius — the text @p theme's when the style gives none: lighter under
/// the pointer or the focus, between the two while held, faded disabled.
[[nodiscard]] GuiStateStyles uiButtonLook(const UiNodeStyle& style,
                                          const GuiTheme& theme);

/// A panel's look from @p style — its fill, border, radius and elevation
/// — when it has a border or is raised; nothing when a flat fill does.
[[nodiscard]] std::optional<GuiStateStyles>
uiPanelLook(const UiNodeStyle& style, const GuiTheme& theme);

/// The font @p style asks for over @p role's in @p theme: its size and
/// weight when it gives them; nothing when it gives neither.
[[nodiscard]] std::optional<GuiFont>
uiTextFont(const UiNodeStyle& style, GuiTextRole role, const GuiTheme& theme);

/// Set @p layout, the covering panel's, to place a screen's root by
/// @p anchor, @p inset from the edges.
void anchorUiRoot(LayoutStyle& layout, UiAnchor anchor, float inset);

}  // namespace eng::game

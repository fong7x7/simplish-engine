#pragma once

/// @file ui-view-widgets.h
/// @brief What a screen view does to a widget of whichever kind it built:
/// its text, and a checkbox's or toggle's state.
/// @par Threading
/// Main-thread-only, with the tree the widget is in.

#include <engine/gui/gui-check-state.h>
#include <engine/gui/gui-widget.h>
#include <functional>
#include <string_view>

namespace eng::game {

/// Show @p text on @p widget — a label's, a button's, a checkbox's or a
/// toggle's; the widget views it, so it must outlive the next change.
void showUiText(GuiWidget& widget, std::string_view text);

/// Set a checkbox or toggle @p widget to @p state — a toggle is on when
/// CHECKED — without telling anyone.
void setUiChecked(GuiWidget& widget, GuiCheckState state);

/// Whether a checkbox or toggle @p widget is on; false for any other.
[[nodiscard]] bool uiChecked(const GuiWidget& widget);

/// Have a checkbox or toggle @p widget call @p on_change when pressed,
/// with whether it is now on.
void onUiCheckChange(GuiWidget& widget, std::function<void(bool)> on_change);

}  // namespace eng::game

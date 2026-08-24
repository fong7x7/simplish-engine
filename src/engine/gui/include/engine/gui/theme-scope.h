#pragma once

#include "gui-widget-id.h"

namespace eng {

struct Theme;  // NOLINT(no-forward-decl) breaks circular: gui-theme.h ->
               // theme-scope-stack.h -> theme-scope.h

/// @thread_safety Main thread only.
struct ThemeScope {
  /// Theme applied to this scope.
  const Theme* theme = nullptr;
  /// Widget subtree root this scope applies to.
  GuiWidgetId root_widget = GUI_WIDGET_ID_INVALID;
};

}  // namespace eng

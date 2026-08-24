#pragma once

/// @file theme-scope-stack.h
/// @brief Scoped theme overrides and token resolution for widget types.
/// @par Threading Main thread only.

#include "gui-widget-type.h"
#include "theme-scope.h"

#include <string_view>
#include <vector>

namespace eng {

/// @thread_safety Main thread only.
class ThemeScopeStack {
public:
  /// Application-wide root theme (fallback for all lookups).
  const Theme* root_theme = nullptr;
  /// Stack of scoped theme overrides, innermost last.
  std::vector<ThemeScope> scopes{};

  /// Push a scoped theme override for a subtree rooted at root_widget.
  void pushScope(const Theme* theme, GuiWidgetId root_widget);

  /// Pop the most recently pushed theme scope.
  void popScope();

  /// Resolve a colour token for a given widget type.
  uint32_t resolveColor(GuiWidgetType type, std::string_view token) const;

  /// Resolve a float token.
  float resolveFloat(GuiWidgetType type, std::string_view token) const;

  /// Resolve an integer token.
  int32_t resolveInt(GuiWidgetType type, std::string_view token) const;

  /// Resolve a string token.
  std::string_view resolveString(GuiWidgetType type,
                                 std::string_view token) const;
};

}  // namespace eng

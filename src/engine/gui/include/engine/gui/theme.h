#pragma once

/// @file theme.h
/// @brief Theme data structure: named token map and per-widget-type overrides.
/// Separated from gui-theme.h to break the circular include chain
/// gui-theme.h -> theme-scope-stack.h -> theme-scope.h -> gui-theme.h.
/// @par Threading Main thread only.

#include "token-value.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace eng {

/// A named collection of design tokens with optional per-widget overrides.
/// @threading Main thread only.
struct Theme {
  /// Theme name (matches JSON filename stem).
  std::string name{};
  /// Global token map keyed by token name.
  std::unordered_map<std::string, TokenValue> tokens{};
  /// Per-widget-type token overrides keyed by widget type name then token
  /// name.
  std::unordered_map<std::string, std::unordered_map<std::string, TokenValue>>
      widget_overrides;
};

/// Parse a theme JSON file. Returns std::nullopt on failure.
std::optional<Theme> loadTheme(std::string_view json_path);

}  // namespace eng

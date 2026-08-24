#pragma once

/// @file gui-theme.h
/// @brief JSON theme tokens and `Theme`; works with `ThemeScopeStack`.
/// @par Threading Main thread only.

#include "theme-scope-stack.h"
#include "token-value.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace eng {

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

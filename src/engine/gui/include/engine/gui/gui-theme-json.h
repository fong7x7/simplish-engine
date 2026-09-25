#pragma once

/// @file gui-theme-json.h
/// @brief Reading a `GuiTheme` from a theme file.
/// @par Threading
/// Any thread; touches nothing shared.

#include "gui-theme.h"

#include <optional>
#include <string>
#include <string_view>

namespace eng {

/// A theme from JSON @p text, or nothing — with why in @p error — when the
/// text is not a theme. The format (see `technical/theming.md`):
///
/// ```json
/// { "name": "Ember", "base": "dark",
///   "palette": { "primary": "#e8703a", "surface": "#221c1a" },
///   "spacing": [0, 2, 4, 8, 12, 16, 24, 32],
///   "radii": [0, 4, 6, 10],
///   "text_sizes": [11, 12, 14, 17, 22, 32],
///   "transition_ms": 150 }
/// ```
///
/// Everything is optional: `base` picks the preset the rest is laid over,
/// and component styles are derived from the result. Colours are `#rgb`,
/// `#rrggbb` or `#rrggbbaa`.
[[nodiscard]] std::optional<GuiTheme> parseGuiTheme(std::string_view text,
                                                    std::string& error);

/// `parseGuiTheme` on the file at @p path.
[[nodiscard]] std::optional<GuiTheme> loadGuiTheme(std::string_view path,
                                                   std::string& error);

}  // namespace eng

#pragma once

/// @file ui-screen-json.h
/// @brief Reading a game screen file.
/// @par Threading
/// Pure.

#include <cstddef>
#include <game/ui/ui-screen-read.h>
#include <string_view>

namespace eng::game {

/// The schema a screen file names: `simplish/ui_screen/1.0`.
inline constexpr std::string_view UI_SCREEN_SCHEMA = "simplish/ui_screen/1.0";

/// How deep a screen's nodes may nest.
inline constexpr size_t UI_SCREEN_MAX_DEPTH = 16;

/// How many nodes a screen may hold.
inline constexpr size_t UI_SCREEN_MAX_NODES = 512;

/// The screen @p text describes — the contents of `content/ui/<id>.ui.json`
/// — named @p id, and every problem with it.
[[nodiscard]] UiScreenRead parseUiScreen(std::string_view text,
                                         std::string_view id);

}  // namespace eng::game

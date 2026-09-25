#pragma once

/// @file ui-text.h
/// @brief A screen's text with its values filled in.
/// @par Threading
/// Pure.

#include <game/ui/ui-values.h>
#include <optional>
#include <string>
#include <string_view>

namespace eng::game {

/// @p pattern with each `{key}` replaced by `values`' `key` — nothing
/// when it has none — and `{{` by a single brace.
[[nodiscard]] std::string fillUiText(std::string_view pattern,
                                     const UiValues& values);

/// @p key's value as a number, or @p key itself read as one; nothing when
/// neither is.
[[nodiscard]] std::optional<float> uiNumber(const UiValues& values,
                                            std::string_view key);

}  // namespace eng::game

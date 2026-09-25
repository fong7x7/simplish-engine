#pragma once

/// @file ui-style-json.h
/// @brief Reading a screen node's layout and colours.
/// @par Threading
/// Pure.

#include "ui-json-read.h"

#include <game/ui/ui-node-style.h>
#include <nlohmann/json.hpp>
#include <string_view>

namespace eng::game {

/// @p node's layout and colours, as a screen file sets them; each key
/// that does not read is a problem in @p read, at @p path, and keeps its
/// default.
[[nodiscard]] UiNodeStyle readUiStyle(const nlohmann::json& node,
                                      std::string_view path, UiJsonRead& read);

/// Whether @p key is one a node may carry: a style key, or `type`, `id`,
/// `text`, `action`, `value`, `max`, `children`.
[[nodiscard]] bool knownUiNodeKey(std::string_view key);

}  // namespace eng::game

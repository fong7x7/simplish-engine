#pragma once

/// @file ui-look-json.h
/// @brief Reading a screen node's look — colours, box, text — and the
/// flags it binds to values.
/// @par Threading
/// Pure.

#include "ui-style-read.h"

#include <game/ui/ui-bindings.h>
#include <game/ui/ui-node-style.h>

namespace eng::game {

/// Read @p s's colours, box and text keys into @p style; each that does
/// not read is a problem, and keeps its default.
void readUiLook(const UiStyleRead& s, UiNodeStyle& style);

/// @p s's `visible`, `disabled`, `selected` and `checked`: each a key, or
/// `!key`. One that is not is a problem, and binds nothing.
[[nodiscard]] UiBindings readUiBindings(const UiStyleRead& s);

}  // namespace eng::game

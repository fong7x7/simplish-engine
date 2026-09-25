#pragma once

/// @file ui-values.h
/// @brief The values a game's screens show.
/// @par Threading
/// A value type.

#include <functional>
#include <map>
#include <string>

namespace eng::game {

/// Every value the game logic has set for its screens to show, by key —
/// ordered, so it reads the same wherever it is walked.
using UiValues = std::map<std::string, std::string, std::less<>>;

}  // namespace eng::game

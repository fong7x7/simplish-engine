#pragma once

/// @file world-ui.h
/// @brief Which of the game's screens are shown, and the values they show.
/// @par Threading
/// A value type.

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace eng::game {

/// What the game logic has asked its screens to be (ADR-012): the ones
/// shown and the values they show. Presentation, kept by the world beside
/// the cues for whoever draws the game, and never hashed: a screen shown
/// changes nothing a tick reads.
struct WorldUi {
  /// The screens shown, by id, in the order shown: the last is on top.
  std::vector<std::string> open{};
  /// Every value set, by key.
  std::map<std::string, std::string, std::less<>> values{};
  /// Counts every change, so whoever draws it rebuilds only when it moves.
  uint64_t revision = 0;
};

}  // namespace eng::game

#pragma once

/// @file deployed-game-options.h
/// @brief How a deployed game is asked to run.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <filesystem>
#include <string>

namespace eng::editor {

/// Ticks a deployed game runs when not told: five minutes of play.
inline constexpr uint64_t DEPLOYED_GAME_DEFAULT_TICKS = 5 * 60 * 60;

/// What `simplish-game`'s command line asks for.
/// @thread_safety Immutable value type.
struct DeployedGameOptions {
  /// The deployed game's content: its manifest, baked levels and data
  /// tables. Empty for the `game/` folder beside the executable.
  std::filesystem::path content{};
  /// The level to play; empty for the one the manifest starts on.
  std::string level{};
  /// Ticks to run at most, if the run is not over first.
  uint64_t max_ticks = DEPLOYED_GAME_DEFAULT_TICKS;
  /// Players in the session, 1 to 4 — every one a stand-in, since nobody
  /// holds the controls of a headless game.
  uint8_t players = 1;
};

}  // namespace eng::editor

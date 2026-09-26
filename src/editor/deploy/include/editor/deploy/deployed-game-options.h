#pragma once

/// @file deployed-game-options.h
/// @brief How a deployed game is asked to run.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <editor/deploy/deployed-game-mode.h>
#include <editor/deploy/deployed-hashes.h>
#include <editor/deploy/deployed-pace.h>
#include <engine/net/lockstep-server-config.h>
#include <engine/net/udp-listen.h>
#include <filesystem>
#include <string>

namespace eng::editor {

/// Ticks a deployed game runs when not told: five minutes of play.
inline constexpr uint64_t DEPLOYED_GAME_DEFAULT_TICKS = 5 * 60 * 60;

/// The most input delay a session may be given: half a second, well
/// inside the input queue's reach (`sim::INPUT_QUEUE_TICKS`).
inline constexpr uint64_t DEPLOYED_MAX_INPUT_DELAY = 30;

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
  /// holds the controls of a headless game. A server, or a host, waits for
  /// this many to be seated before it starts the run.
  uint8_t players = 1;
  /// Whether every tick's hash is kept.
  DeployedHashes hashes = DeployedHashes::LAST_ONLY;
  /// Alone, or which end of a co-op session (ADR-013).
  DeployedGameMode mode = DeployedGameMode::SOLO;
  /// The UDP port to serve or host on, or the server's when joining.
  uint16_t port = net::UDP_DEFAULT_PORT;
  /// The server to join: a host name or address.
  std::string address{};
  /// Ticks of input delay a server gives its session (ADR-005).
  uint8_t input_delay = net::NET_DEFAULT_INPUT_DELAY;
  /// What paces a networked run's input.
  DeployedPace pace = DeployedPace::REAL_TIME;
};

}  // namespace eng::editor

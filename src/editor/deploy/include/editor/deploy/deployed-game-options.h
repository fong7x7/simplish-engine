#pragma once

/// @file deployed-game-options.h
/// @brief How a deployed game is asked to run.
/// @par Threading Thread-safe (immutable value type).

#include <chrono>
#include <cstdint>
#include <editor/deploy/deployed-game-mode.h>
#include <editor/deploy/deployed-hashes.h>
#include <editor/deploy/deployed-pace.h>
#include <engine/net/net-lan-game.h>
#include <engine/net/udp-listen.h>
#include <filesystem>
#include <optional>
#include <string>

namespace eng::editor {

/// Ticks a deployed game runs when not told: five minutes of play.
inline constexpr uint64_t DEPLOYED_GAME_DEFAULT_TICKS = 5 * 60 * 60;

/// The longest `--stall-drop` a server may be given, in seconds: an hour.
inline constexpr uint64_t DEPLOYED_MAX_STALL_DROP_S = 3600;

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
  /// Ticks of input delay a server gives its session (ADR-005); nothing
  /// to choose it at each start from the worst round trip measured.
  std::optional<uint8_t> input_delay{};
  /// What paces a networked run's input.
  DeployedPace pace = DeployedPace::REAL_TIME;
  /// Where to write the run's replay; empty for nowhere. A server writes
  /// its reference run's, a client its own.
  std::filesystem::path replay{};
  /// The replay `VERIFY` plays back.
  std::filesystem::path verify{};
  /// Where a server writes a desync's report; empty for the working
  /// directory.
  std::filesystem::path desync_dir{};
  /// How long a server's run waits on a seat before dropping it and
  /// playing it with a stand-in: long enough that only a client that has
  /// stopped sending input for good trips it.
  std::chrono::milliseconds stall_drop = std::chrono::seconds(10);
  /// What a server calls its session to players looking for one on the
  /// LAN; empty for the deployed game's name.
  std::string name{};
  /// The UDP port servers answer LAN queries on, and players ask on.
  uint16_t lan_port = net::NET_LAN_PORT;
  /// The session's password: a server admits only clients that give it, a
  /// client gives it. Empty for an open session.
  std::string password{};
};

}  // namespace eng::editor

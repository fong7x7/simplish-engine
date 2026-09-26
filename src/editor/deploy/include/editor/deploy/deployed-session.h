#pragma once

/// @file deployed-session.h
/// @brief A deployed game as one end of a co-op session, over UDP.
/// @par Threading Main-thread-only; blocks until the run is over.

#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game-run.h>
#include <game/logic/game-logic-factory.h>
#include <ostream>

namespace eng::editor {

/// Run the deployed game @p options describes as a co-op session over UDP
/// (ADR-013), by its mode:
///
/// - `SERVE` — a dedicated server on `port`: seats clients, starts the run
///   once `players` are in, simulates it as the reference their hashes are
///   checked against, and ends it when it is over, after `max_ticks`, or
///   when everyone has left. The result is the reference run's.
/// - `HOST` — the same server, relaying only, with this process's own
///   player joined to it; `players` counts that player. The result is that
///   player's run.
/// - `JOIN` — a player in the session at `address`:`port`, until the
///   server ends the run. The result is this player's run.
///
/// Every local player is a stand-in, since nobody holds the controls of a
/// headless game; input is sampled at `pace`. What happens goes to @p out;
/// a run that could not start, or stopped short — refused, desynced, the
/// server gone — says why in its `error`.
[[nodiscard]] DeployedGameRun
runDeployedSession(const DeployedGameOptions& options,
                   game::GameLogicFactory logic, std::ostream& out);

}  // namespace eng::editor

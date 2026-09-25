#pragma once

/// @file deployed-game.h
/// @brief A project's deployed game, run headless from what a deploy baked.
/// @par Threading Main-thread-only.

#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game-run.h>
#include <game/logic/game-logic-factory.h>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace eng::editor {

/// Run the deployed game @p options describes: read its manifest, the
/// setup baked for the level and the data tables beside it; build the
/// world with an instance of the logic @p logic makes, if it makes one;
/// and step it — every player a stand-in — until the run is over or the
/// ticks run out. What the logic says, and the result, go to @p out.
///
/// What `simplish-game` is today (ADR-011): the deterministic simulation
/// of a project with its rules linked in, and no window — the dedicated
/// host and CI run of Engine §5, and the proof a deploy works. The
/// rendered client that puts a player at the controls is still to come;
/// it will start its runs from the same baked setups.
[[nodiscard]] DeployedGameRun
runDeployedGame(const DeployedGameOptions& options,
                game::GameLogicFactory logic, std::ostream& out);

/// The options `simplish-game`'s arguments — the program name excluded —
/// ask for: `--content DIR`, `--level ID`, `--ticks N`, `--players N`.
/// Nothing when one is not understood.
[[nodiscard]] std::optional<DeployedGameOptions>
parseDeployedGameArgs(std::span<const std::string_view> args);

}  // namespace eng::editor

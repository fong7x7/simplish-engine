#pragma once

/// @file deployed-replay.h
/// @brief A deployed game's replays: written after a run, played back after.
/// @par Threading Main-thread-only (reads and writes the disk).

#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game-run.h>
#include <engine/sim/replay.h>
#include <filesystem>
#include <game/logic/game-logic-factory.h>
#include <optional>
#include <ostream>

namespace eng::editor {

/// Write @p replay where @p options asks, if it asks, saying so to @p out.
void saveReplay(const DeployedGameOptions& options, const sim::Replay& replay,
                std::ostream& out);

/// The replay in the file at @p path; nothing when it cannot be read or
/// is not a replay.
[[nodiscard]] std::optional<sim::Replay>
readReplayFile(const std::filesystem::path& path);

/// Play the replay at `options.verify` back against the deployed content,
/// with the logic @p logic makes, and say whether it reproduces: the run's
/// `error` names the tick and section it first diverges on, or why it
/// could not be played. Its hash is the recording's last checkpoint.
[[nodiscard]] DeployedGameRun
verifyDeployedReplay(const DeployedGameOptions& options,
                     game::GameLogicFactory logic);

}  // namespace eng::editor

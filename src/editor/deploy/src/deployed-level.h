#pragma once

/// @file deployed-level.h
/// @brief Which level a deployed game plays, and its baked setup.
/// @par Threading Main-thread-only (reads the disk).

#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game-run.h>
#include <game/world/game-setup.h>
#include <optional>

namespace eng::editor {

/// The setup of the level @p options asks for — the manifest's start
/// level when it asks for none — naming it in @p run; nothing, with
/// @p run's error saying why, when there is none.
[[nodiscard]] std::optional<game::GameSetup>
manifestSetup(const DeployedGameOptions& options, DeployedGameRun& run);

}  // namespace eng::editor

#pragma once

/// @file player-change.h
/// @brief A downed player revived or put out, as it happened.
/// @par Threading
/// A value type.

#include <engine/sim/entity-handle.h>
#include <game/player/player-change-kind.h>
#include <optional>

namespace eng::game {

/// One downed player's tick coming to something: what `updateDownedPlayers`
/// reports, so whoever listens hears it where it happened rather than
/// working it out from the pool afterwards.
struct PlayerChange {
  /// What became of them.
  PlayerChangeKind kind = PlayerChangeKind::REVIVED;
  /// Whom.
  sim::EntityHandle player{};
  /// For a revive, the teammate who stood by them.
  std::optional<sim::EntityHandle> by{};
};

}  // namespace eng::game

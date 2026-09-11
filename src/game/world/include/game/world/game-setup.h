#pragma once

/// @file game-setup.h
/// @brief Everything a game starts from, besides its content.
/// @par Threading
/// A value type.

#include <array>
#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/physics/collision-box.h>
#include <engine/sim/tick-input.h>
#include <game/actors/actor-spawn.h>
#include <string>
#include <vector>

namespace eng::game {

/// The initial conditions of a run: how many players, who each one plays as,
/// where each one enters the level, what in it they cannot walk through, and
/// the actors — enemies and NPCs — waiting in it.
/// With the seed and the input stream, this is what the simulation is a
/// function of (ADR-002).
///
/// Plain positions rather than the editor's player starts: the game never
/// sees an editor type. Whoever starts a run — the editor's playtest, the
/// client, a CI test — turns what it has into this. It is the shape ADR-008's
/// entry state will grow from.
struct GameSetup {
  /// The session seed every RNG stream is derived from.
  uint64_t seed = 0;
  /// Players in the session, 1 to `sim::MAX_PLAYERS`.
  uint8_t player_count = 1;
  /// Where each player's feet land, by input slot. Entries at or beyond
  /// `player_count` are unused.
  std::array<Vec3, sim::MAX_PLAYERS> spawns{};
  /// Which character each player plays as, by input slot: an id in the
  /// content's character table, or empty for the default character. The
  /// "build" of ADR-008's entry state, and what a replay records so a run
  /// replays with the stats it was played with.
  std::array<std::string, sim::MAX_PLAYERS> characters{};
  /// The level's solid geometry, in the order the level holds it. Fixed for
  /// the run; nothing a tick does moves it.
  std::vector<physics::CollisionBox> obstacles;
  /// The actors the level starts with, in the order the level holds them.
  /// That order is their dense order, so it is part of the run: two peers
  /// listing the same actors differently simulate different runs.
  std::vector<ActorSpawn> actors;
};

}  // namespace eng::game

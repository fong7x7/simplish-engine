#pragma once

/// @file player-system.h
/// @brief What the simulation does to players each tick.
/// @par Threading
/// Main-thread-only; called from the tick's phases.

#include <cstdint>
#include <engine/math/vec3.h>
#include <engine/sim/entity-handle.h>
#include <engine/sim/state-hasher.h>
#include <engine/sim/tick-input.h>
#include <game/player/player-pool.h>
#include <optional>

namespace eng::game {

/// How far a player at full stick moves in one tick: five tiles a second.
///
/// A constant until the data tables of Game REQUIREMENTS §3 exist; a
/// build's movement speed then comes from content instead.
inline constexpr float PLAYER_SPEED_TILES_PER_TICK = 5.0F / 60.0F;

/// Add a player driven by input slot @p input_slot, feet at @p at, aiming
/// along world +X. Nothing when the pool is full.
std::optional<sim::EntityHandle> spawnPlayer(PlayerPool& pool,
                                             uint8_t input_slot, Vec3 at);

/// §4.1 step 2: move every player by its stick and take up its aim.
///
/// Movement is in world X and Y, at `PLAYER_SPEED_TILES_PER_TICK` for a
/// full stick and proportionally less for a partial one. Height is not
/// touched: there is no ground to follow yet, so a player stays at the
/// height they spawned at. Nothing collides yet either — that waits on
/// `engine/physics`.
void movePlayers(PlayerPool& pool, const sim::TickInput& input);

/// §4.1 step 8: destroy the players marked for it.
void compactPlayers(PlayerPool& pool);

/// Fold every player's state into a tick hash section.
void hashPlayers(const PlayerPool& pool, sim::StateHasher& hasher);

}  // namespace eng::game

#pragma once

/// @file event-queries.h
/// @brief Asking the last tick's events what happened.
/// @par Threading
/// Main-thread-only; the logic's own thread, inside its tick.

#include <cstdint>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-event.h>
#include <game/sdk/event-filter.h>
#include <vector>

namespace eng::game::sdk {

/// Every event of the last tick passing @p filter, in the order they
/// happened. Their ids and states view the world's, valid for this tick.
[[nodiscard]] std::vector<LogicEvent> findEvents(const GameLogicWorld& world,
                                                 const EventFilter& filter);

/// How many of the last tick's events pass @p filter.
[[nodiscard]] uint32_t countEvents(const GameLogicWorld& world,
                                   const EventFilter& filter);

/// Whether any of the last tick's events passes @p filter.
[[nodiscard]] bool heard(const GameLogicWorld& world,
                         const EventFilter& filter);

/// The health every event passing @p filter took, summed: the damage a
/// player dealt last tick, or a boss took from blasts.
[[nodiscard]] uint32_t totalAmount(const GameLogicWorld& world,
                                   const EventFilter& filter);

}  // namespace eng::game::sdk

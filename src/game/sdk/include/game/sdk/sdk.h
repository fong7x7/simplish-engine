#pragma once

/// @file sdk.h
/// @brief The Simplish game logic SDK, whole: include this one header.
/// @par Threading
/// See each header.
///
/// A project's game logic is written against `eng::game::GameLogicWorld`
/// (what the engine lets it read and change) and this SDK (what makes that
/// pleasant): a `Game` base with event hooks, queries over players and
/// actors, per-entity data, timers and phases, spawn patterns and dice.
/// docs/game/sdk.md in the engine walks through it.

#include <game/logic/game-logic-entry.h>  // IWYU pragma: export
#include <game/logic/game-logic.h>        // IWYU pragma: export
#include <game/sdk/actor-filter.h>        // IWYU pragma: export
#include <game/sdk/actor-queries.h>       // IWYU pragma: export
#include <game/sdk/cooldown.h>            // IWYU pragma: export
#include <game/sdk/entity-data.h>         // IWYU pragma: export
#include <game/sdk/event-filter.h>        // IWYU pragma: export
#include <game/sdk/event-queries.h>       // IWYU pragma: export
#include <game/sdk/events.h>              // IWYU pragma: export
#include <game/sdk/every.h>               // IWYU pragma: export
#include <game/sdk/game.h>                // IWYU pragma: export
#include <game/sdk/logic-tests.h>         // IWYU pragma: export
#include <game/sdk/phase.h>               // IWYU pragma: export
#include <game/sdk/player-input.h>        // IWYU pragma: export
#include <game/sdk/player-queries.h>      // IWYU pragma: export
#include <game/sdk/random.h>              // IWYU pragma: export
#include <game/sdk/ring-spawn.h>          // IWYU pragma: export
#include <game/sdk/schedule.h>            // IWYU pragma: export
#include <game/sdk/ticks.h>               // IWYU pragma: export
#include <game/sdk/weapon.h>              // IWYU pragma: export

#pragma once

/// @file world-read-view.h
/// @brief The world between ticks, through the interface game logic reads.
/// @par Threading
/// Main-thread-only; valid while the world is not stepped.

#include "world-logic-view.h"
#include "world-read-scratch.h"
#include "world-read-sources.h"

namespace eng::game {

/// A `GameLogicWorld` over a world between ticks: reads see it as the last
/// tick left it; writes, spawns and dice go to a scratch of its own and
/// change nothing. What a logic test reads the world through.
class WorldReadView final : private WorldReadScratch, public WorldLogicView {
public:
  /// A view of @p sources, which must outlive it.
  explicit WorldReadView(const WorldReadSources& sources);
};

}  // namespace eng::game

#pragma once

/// @file world-logic-hash.h
/// @brief A tick hash section, behind the interface game logic hashes into.
/// @par Threading
/// Main-thread-only; lives for one `hashState`.

#include <cstddef>
#include <engine/sim/state-hasher.h>
#include <game/logic/game-logic-hash.h>
#include <span>

namespace eng::game {

/// `GameLogicHash` folding into one section of the world's tick hash.
class WorldLogicHash final : public GameLogicHash {
public:
  /// Folds into @p hasher, which must outlive this.
  explicit WorldLogicHash(sim::StateHasher& hasher) : hasher_(&hasher) {}

  void addBytes(std::span<const std::byte> bytes) override {
    hasher_->addBytes(bytes);
  }

private:
  /// The section being folded into.
  sim::StateHasher* hasher_;
};

}  // namespace eng::game

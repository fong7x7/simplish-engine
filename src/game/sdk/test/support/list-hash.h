#pragma once

/// @file list-hash.h
/// @brief A hash that keeps what was folded into it, for tests to compare.
/// @par Threading
/// Main-thread-only.

#include <cstddef>
#include <game/logic/game-logic-hash.h>
#include <span>
#include <vector>

namespace eng::game::sdk::test {

/// Hashes into a list, so a test can see what was folded in.
class ListHash final : public GameLogicHash {
public:
  void addBytes(std::span<const std::byte> bytes) override {
    added.insert(added.end(), bytes.begin(), bytes.end());
  }
  /// Every byte folded in.
  std::vector<std::byte> added;
};

}  // namespace eng::game::sdk::test

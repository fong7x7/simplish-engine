#pragma once

/// @file game-logic-hash.h
/// @brief Where game logic folds its own state into the tick hash.
/// @par Threading
/// Main-thread-only; valid only during `GameLogic::hashState`.

#include <cstddef>
#include <span>
#include <type_traits>

namespace eng::game {

/// The tick hash's "logic" section, as game logic sees it. Everything a
/// logic keeps from one tick to the next and decides anything by is
/// simulation state, and has to be folded in here — or a peer that
/// diverged in it would go unnoticed until its divergence reached a player
/// (ADR-002).
///
/// An interface rather than the engine's hasher, so a project's logic links
/// against nothing of the engine's: the call crosses into the host.
class GameLogicHash {
public:
  virtual ~GameLogicHash() = default;

  /// Folds @p bytes in, in order.
  virtual void addBytes(std::span<const std::byte> bytes) = 0;

  /// Folds one value in: an integer, a float, an enum, or a struct of them
  /// with no padding.
  template <typename T>
    requires std::has_unique_object_representations_v<T> ||
             std::is_floating_point_v<T>
  void add(const T& value) {
    addBytes(std::as_bytes(std::span<const T, 1>(&value, 1)));
  }

  GameLogicHash(const GameLogicHash&) = delete;
  GameLogicHash& operator=(const GameLogicHash&) = delete;
  GameLogicHash(GameLogicHash&&) = delete;
  GameLogicHash& operator=(GameLogicHash&&) = delete;

protected:
  GameLogicHash() = default;
};

}  // namespace eng::game

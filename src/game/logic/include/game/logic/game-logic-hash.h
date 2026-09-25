#pragma once

/// @file game-logic-hash.h
/// @brief Where game logic folds its own state into the tick hash.
/// @par Threading
/// Main-thread-only; valid only during `GameLogic::hashState`.

#include <cstddef>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <span>
#include <type_traits>

namespace eng::game {

// A position's bytes are its floats, with nothing between them.
static_assert(sizeof(Vec2) == 2 * sizeof(float));
static_assert(sizeof(Vec3) == 3 * sizeof(float));

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

  /// Folds one value in: an integer, a float, an enum, a position
  /// (`Vec2`, `Vec3`), or a struct of integers with no padding.
  template <typename T>
    requires std::has_unique_object_representations_v<T> ||
             std::is_floating_point_v<T> || std::is_same_v<T, Vec2> ||
             std::is_same_v<T, Vec3>
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

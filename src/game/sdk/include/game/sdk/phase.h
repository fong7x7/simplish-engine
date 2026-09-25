#pragma once

/// @file phase.h
/// @brief Which stage of its own a game is in, and since when.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/logic/game-logic-hash.h>

namespace eng::game::sdk {

/// A small state machine for the logic's own stages — a boss's phases, a
/// level's build-up, calm and siege — over an enum of the logic's own:
///
/// @code
///   enum class Stage : uint8_t { CALM, SIEGE, BOSS };
///   sdk::Phase<Stage> stage_;
///   if (stage_.is(Stage::CALM) && stage_.age(world.tick()) > sdk::seconds(30))
///     stage_.enter(Stage::SIEGE, world.tick());
/// @endcode
template <typename Stage> struct Phase {
  /// The stage it is in.
  Stage current{};
  /// The tick it entered it on.
  uint64_t since = 0;

  /// Whether it is in @p stage.
  [[nodiscard]] constexpr bool is(Stage stage) const {
    return current == stage;
  }
  /// Enter @p next on @p tick — anew, even when it is already there.
  constexpr void enter(Stage next, uint64_t tick) {
    current = next;
    since = tick;
  }
  /// Ticks it has been in its stage, on @p tick.
  [[nodiscard]] constexpr uint64_t age(uint64_t tick) const {
    return tick - since;
  }
  /// Fold it into @p hash.
  void hashInto(GameLogicHash& hash) const {
    hash.add(current);
    hash.add(since);
  }
};

}  // namespace eng::game::sdk

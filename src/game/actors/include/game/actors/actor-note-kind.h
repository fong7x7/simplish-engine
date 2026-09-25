#pragma once

/// @file actor-note-kind.h
/// @brief What an actor did that is worth telling anyone of.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The moments of an actor's own thinking that whoever runs the actors can
/// hear of: what an `ActorNote` reports.
enum class ActorNoteKind : uint8_t {
  /// It went into another state of its behavior.
  STATE_ENTERED,
  /// It took someone new as its target: a player, or an opponent.
  NOTICED,
  /// It attacked: struck, fired, spat, or blew itself up.
  ATTACKED,
  /// It began an attack that winds up, to land `windup_ticks` later.
  WINDING_UP,
};

}  // namespace eng::game

#pragma once

/// @file behavior-definition.h
/// @brief A behavior: an actor's senses, movement, and state machine.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/content/behavior-exit.h>
#include <game/content/behavior-movement.h>
#include <game/content/behavior-senses.h>
#include <game/content/behavior-state.h>
#include <string>
#include <vector>

namespace eng::game {

/// Most states one behavior may have: an index into them is a byte, and a
/// machine larger than this is a sign of one trying to be a program.
inline constexpr uint8_t BEHAVIOR_MAX_STATES = 32;

/// The intelligence an actor runs (ADR-009): a small state machine written
/// as data, each state one action from a closed set, each way out one
/// condition from another.
///
/// **Interrupts** are exits tested before the current state's own, in
/// every state — how a behavior says "whatever else is happening, when
/// this happens, do that". One leading to the state the actor is already
/// in is skipped, so an interrupt never restarts its own state.
///
/// One record whichever loader filled it (ADR-007): the editor reads
/// `content/data/behaviors.data.json` into these, the built-in presets are
/// written as these, and a shipping build will generate them.
struct BehaviorDefinition {
  /// Stable identifier, as a prop and a replay name it: `guard`.
  std::string id{};
  /// What the editor's Behavior row calls it: `Guard`.
  std::string name{};
  /// How it perceives.
  BehaviorSenses senses{};
  /// How it moves.
  BehaviorMovement movement{};
  /// The state an actor starts in, as an index into `states`.
  uint8_t initial = 0;
  /// Exits tested before the current state's own, in every state.
  std::vector<BehaviorExit> interrupts{};
  /// The states, at least one.
  std::vector<BehaviorState> states{};
};

}  // namespace eng::game

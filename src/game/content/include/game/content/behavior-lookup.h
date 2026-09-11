#pragma once

/// @file behavior-lookup.h
/// @brief The built-in behaviors, and finding a behavior by id.
/// @par Threading
/// Pure functions over value types.

#include <game/content/behavior-action.h>
#include <game/content/behavior-definition.h>
#include <game/content/behavior-state.h>
#include <game/content/game-content.h>
#include <span>
#include <string_view>

namespace eng::game {

/// The behaviors every project has without writing any: `idle`, `wander`,
/// `guard`, `chase`, `skirmisher`, `coward`, `follower` and `charger`, in
/// that order. A project's own behavior with the same id replaces one.
[[nodiscard]] std::span<const BehaviorDefinition> builtInBehaviors();

/// The behavior @p id names: the project's own in @p content first, then a
/// built-in. An id that names neither, or a behavior that is not
/// `behaviorIsWellFormed`, resolves to the built-in `idle`.
///
/// Never a failure, like `resolveCharacter`: a prop naming a behavior the
/// content lacks is still an actor, and every peer with the same content
/// resolves it to the same thing.
[[nodiscard]] const BehaviorDefinition&
resolveBehavior(const GameContent& content, std::string_view id);

/// Whether @p behavior can be run as it is: at least one state and at most
/// `BEHAVIOR_MAX_STATES`, and its initial state and every exit's target
/// within them.
[[nodiscard]] bool behaviorIsWellFormed(const BehaviorDefinition& behavior);

/// A state doing @p action, with the action's default distances: how short
/// a pursuit stops, how far a wander strays. What a loader starts from
/// before reading what a file says.
[[nodiscard]] BehaviorState defaultBehaviorState(BehaviorAction action);

}  // namespace eng::game

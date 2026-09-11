#pragma once

/// @file editor-behavior-choices.h
/// @brief What a prop's Behavior and Faction rows offer, and turning a
/// behavior reference into the behavior it names.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <game/content/behavior-definition.h>
#include <game/content/faction.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// What the Behavior row calls a prop with none: it is scenery, not an
/// actor.
inline constexpr std::string_view EDITOR_BEHAVIOR_NONE_NAME = "None";

/// The behaviors a prop can run, in the order the Behavior row steps
/// through them, and which of them it runs now.
/// @thread_safety Immutable value type.
struct EditorBehaviorChoices {
  /// What the row shows for each choice: none first, then every behavior
  /// by name.
  std::vector<std::string> names{};
  /// The reference each choice writes into the prop, alongside `names`:
  /// empty for none, `behavior:<id>` otherwise.
  std::vector<std::string> refs{};
  /// Which choice the prop has now.
  size_t current = 0;
};

/// Every behavior a project can name, in the order it is offered: the
/// built-in presets, each replaced by the project's own of the same id,
/// then the project's own that replace none, in file order.
[[nodiscard]] std::vector<const game::BehaviorDefinition*>
editorAvailableBehaviors(const std::vector<game::BehaviorDefinition>& project);

/// Every behavior of `editorAvailableBehaviors`, with @p behavior — a
/// prop's reference — picked. A reference naming none of them is offered
/// too, last, as `<ref> (missing)`, as the Character row does.
[[nodiscard]] EditorBehaviorChoices
editorBehaviorChoices(const std::vector<game::BehaviorDefinition>& project,
                      const std::string& behavior);

/// How a prop references the behavior @p id: `behavior:guard`.
[[nodiscard]] std::string editorBehaviorRef(std::string_view id);

/// The behavior id a prop's reference @p ref names — `guard` for
/// `behavior:guard` — or empty when it is empty or references anything but
/// a behavior.
[[nodiscard]] std::string editorBehaviorIdOf(std::string_view ref);

/// The behavior @p ref names among what @p project can name, or nothing.
[[nodiscard]] const game::BehaviorDefinition*
findEditorBehavior(const std::vector<game::BehaviorDefinition>& project,
                   std::string_view ref);

/// What the Faction row calls @p faction: `Hostile`.
[[nodiscard]] std::string_view editorFactionName(game::Faction faction);

/// What the Faction row offers, in the order it steps through them.
[[nodiscard]] std::vector<std::string> editorFactionNames();

}  // namespace eng::editor

#pragma once

/// @file editor-route-choices.h
/// @brief What an actor's Route row offers.
/// @par Threading Immutable value type.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eng::editor {

/// The routes an actor can patrol, in the order the Route row steps
/// through them, and which of them it patrols now.
/// @thread_safety Immutable value type.
struct EditorRouteChoices {
  /// What the row shows for each choice: `None` first, then `Route N` for
  /// every route the level has a waypoint of.
  std::vector<std::string> names{};
  /// The route each choice writes into the prop, alongside `names`: 0 for
  /// none.
  std::vector<uint8_t> routes{};
  /// Which choice the prop has now.
  size_t current = 0;
};

}  // namespace eng::editor

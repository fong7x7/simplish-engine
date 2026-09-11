#pragma once

/// @file editor-choice-kind.h
/// @brief Which of the properties panel's choice rows is which.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

namespace eng::editor {

/// A row of the properties panel that picks one name from a list rather
/// than holding a number: a rigged model's clip, a start's character, a
/// prop's behavior and its faction.
///
/// Named by kind rather than by position, because which rows a selection
/// has depends on what it is — a rigged prop with a behavior has three, a
/// static prop without one has one — and an edit must reach the row it
/// came from whichever of them are showing.
/// @thread_safety Immutable value type.
enum class EditorChoiceKind : uint8_t {
  /// The animation clip a rigged prop plays.
  ANIMATION,
  /// The character a player start's player plays as.
  CHARACTER,
  /// The behavior a prop runs in a playtest, which makes it an actor.
  BEHAVIOR,
  /// Which side an actor is on.
  FACTION,
  /// The patrol route an actor walks.
  ROUTE,
};

/// What each kind's row is labelled, in enumerator order.
inline constexpr std::string_view EDITOR_CHOICE_LABELS[] = {
    "Animation", "Character", "Behavior", "Faction", "Route"};

static_assert(std::size(EDITOR_CHOICE_LABELS) ==
                  static_cast<size_t>(EditorChoiceKind::ROUTE) + 1,
              "every choice row needs a label");

/// What @p kind's row is labelled.
[[nodiscard]] constexpr std::string_view
editorChoiceLabel(EditorChoiceKind kind) {
  return EDITOR_CHOICE_LABELS[static_cast<size_t>(kind)];
}

}  // namespace eng::editor

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
  /// The preset a particle emitter's burst was started from.
  EFFECT,
  /// The sprite sheet a billboard shows.
  SHEET,
  /// What a step on a prop sounds like, in place of the ground under it.
  SURFACE,
  /// What an actor's feet sound like.
  FOOTSTEPS,
  /// The terrain a selected area of ground is painted with.
  TERRAIN,
  /// How deep a selected area of water is.
  WATER_DEPTH,
};

/// What each kind's row is labelled, in enumerator order.
inline constexpr std::string_view EDITOR_CHOICE_LABELS[] = {
    "Animation", "Character", "Behavior",  "Faction", "Route", "Effect",
    "Sheet",     "Surface",   "Footsteps", "Terrain", "Depth"};

static_assert(std::size(EDITOR_CHOICE_LABELS) ==
                  static_cast<size_t>(EditorChoiceKind::WATER_DEPTH) + 1,
              "every choice row needs a label");

/// What @p kind's row is labelled.
[[nodiscard]] constexpr std::string_view
editorChoiceLabel(EditorChoiceKind kind) {
  return EDITOR_CHOICE_LABELS[static_cast<size_t>(kind)];
}

/// Whether @p kind's row goes above the property rows rather than below.
///
/// An emitter's Effect row is the one that replaces every number under it,
/// and there are enough of those that the panel scrolls: at the top, it is
/// the first thing seen and never the last thing scrolled to. A
/// billboard's Sheet row leads for the same reason — every number under it
/// describes the sheet it picks, and a billboard with no sheet yet shows
/// nothing until that row is used.
[[nodiscard]] constexpr bool editorChoiceLeads(EditorChoiceKind kind) {
  return kind == EditorChoiceKind::EFFECT || kind == EditorChoiceKind::SHEET;
}

}  // namespace eng::editor

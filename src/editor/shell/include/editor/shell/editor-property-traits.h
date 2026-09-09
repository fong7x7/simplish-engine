#pragma once

/// @file editor-property-traits.h
/// @brief What each property is called, and what sort of number it holds.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-property-field.h>
#include <iterator>
#include <string_view>

namespace eng::editor {

/// The sort of number a property holds.
///
/// Stepping, scrubbing, formatting and clamping are decided from this
/// rather than from the field itself: there are five kinds and fourteen
/// fields, and a rule written per kind cannot disagree with itself about
/// two fields that hold the same sort of number.
/// @thread_safety Immutable value type.
enum class EditorPropertyKind : uint8_t {
  /// A length in tiles, which may be negative: a position is one.
  DISTANCE,
  /// A length in tiles that cannot be: how far a light reaches is one, and
  /// a negative reach is not a shorter one, it is a nonsense.
  EXTENT,
  /// Degrees, wrapped into [-180, 180).
  ANGLE,
  /// One component of a direction, clamped to [-1, 1].
  AXIS,
  /// A non-negative multiplier.
  FACTOR,
  /// A fraction, clamped to [0, 1].
  UNIT,
};

/// One property's fixed description.
/// @thread_safety Immutable value type.
struct EditorPropertyTraits {
  /// Row label. Spelled out per row rather than grouped under headings:
  /// unambiguous labels read the same and cost the panel no rows a pointer
  /// cannot hit.
  std::string_view label;
  /// What sort of number the row holds.
  EditorPropertyKind kind;
};

/// Every field's traits, indexed by the field's own value.
///
/// A table rather than a switch: fourteen arms of two lines each says no
/// more than fourteen rows of one, and the assertion below catches the
/// field added to the enum without an entry here.
inline constexpr EditorPropertyTraits EDITOR_PROPERTY_TRAITS[] = {
    {"Position X", EditorPropertyKind::DISTANCE},
    {"Position Y", EditorPropertyKind::DISTANCE},
    {"Position Z", EditorPropertyKind::DISTANCE},
    {"Rotation X", EditorPropertyKind::ANGLE},
    {"Rotation Y", EditorPropertyKind::ANGLE},
    {"Rotation Z", EditorPropertyKind::ANGLE},
    {"Direction X", EditorPropertyKind::AXIS},
    {"Direction Y", EditorPropertyKind::AXIS},
    {"Direction Z", EditorPropertyKind::AXIS},
    {"Colour R", EditorPropertyKind::UNIT},
    {"Colour G", EditorPropertyKind::UNIT},
    {"Colour B", EditorPropertyKind::UNIT},
    {"Intensity", EditorPropertyKind::FACTOR},
    {"Range", EditorPropertyKind::EXTENT},
};

static_assert(std::size(EDITOR_PROPERTY_TRAITS) ==
                  std::size(EDITOR_ALL_PROPERTY_FIELDS),
              "every property field needs a label and a kind");

/// Label and kind of one field.
[[nodiscard]] constexpr const EditorPropertyTraits&
editorPropertyTraits(EditorPropertyField field) {
  return EDITOR_PROPERTY_TRAITS[static_cast<size_t>(field)];
}

/// Row label for a field.
[[nodiscard]] constexpr std::string_view
editorPropertyFieldLabel(EditorPropertyField field) {
  return editorPropertyTraits(field).label;
}

/// What sort of number a field holds.
[[nodiscard]] constexpr EditorPropertyKind
editorPropertyFieldKind(EditorPropertyField field) {
  return editorPropertyTraits(field).kind;
}

/// Whether a field is an angle, which decides how it is written out and
/// whether it wraps.
[[nodiscard]] constexpr bool
editorPropertyFieldIsAngle(EditorPropertyField field) {
  return editorPropertyFieldKind(field) == EditorPropertyKind::ANGLE;
}

}  // namespace eng::editor

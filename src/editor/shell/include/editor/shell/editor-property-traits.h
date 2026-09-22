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
/// rather than from the field itself: there are thirteen kinds and
/// forty-seven fields, and a rule written per kind cannot disagree with itself
/// about two fields that hold the same sort of number.
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
  /// A whole number from 1 to the field's `maximum`: a player, a route, a
  /// place in a route.
  COUNT,
  /// On or off, held as 1 or 0: the panel draws a checkbox for it, and a
  /// click flips it rather than stepping it.
  TOGGLE,
  /// A size multiplier, clamped to [`EDITOR_SCALE_MIN`, `EDITOR_SCALE_MAX`]:
  /// the panel draws a slider for it, laid out logarithmically so halving
  /// and doubling are the same distance either side of 1.
  SCALE,
  /// A time in seconds, which cannot be negative: how long a particle
  /// lives. Stepped in twentieths, since the quickest effects last a few.
  DURATION,
  /// Degrees either side of a direction, clamped to [0, 180]: 180 is every
  /// way. Not an `ANGLE`, which would wrap 180 round to -180.
  HALF_ANGLE,
  /// A small length that cannot be negative, stepped in hundredths: a
  /// particle's radius, a few hundredths of a tile.
  FINE,
  /// An acceleration, either way: gravity, negative for what rises.
  ACCELERATION,
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
  /// The most a `COUNT` row may hold; unused by every other kind.
  float maximum = 0.0f;
};

/// Every field's traits, indexed by the field's own value.
///
/// A table rather than a switch: sixteen arms of two lines each says no
/// more than sixteen rows of one, and the assertion below catches the
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
    {"Player", EditorPropertyKind::COUNT, 4.0f},
    {"Collides", EditorPropertyKind::TOGGLE},
    {"Scale", EditorPropertyKind::SCALE},
    {"Route", EditorPropertyKind::COUNT, 9.0f},
    {"Order", EditorPropertyKind::COUNT, 99.0f},
    {"Interval", EditorPropertyKind::FACTOR},
    {"Particles", EditorPropertyKind::COUNT, 200.0f},
    {"Spread", EditorPropertyKind::HALF_ANGLE},
    {"Speed Min", EditorPropertyKind::EXTENT},
    {"Speed Max", EditorPropertyKind::EXTENT},
    {"Life Min", EditorPropertyKind::DURATION},
    {"Life Max", EditorPropertyKind::DURATION},
    {"Size Start", EditorPropertyKind::FINE},
    {"Size End", EditorPropertyKind::FINE},
    {"Start R", EditorPropertyKind::UNIT},
    {"Start G", EditorPropertyKind::UNIT},
    {"Start B", EditorPropertyKind::UNIT},
    {"Start Hide", EditorPropertyKind::UNIT},
    {"End R", EditorPropertyKind::UNIT},
    {"End G", EditorPropertyKind::UNIT},
    {"End B", EditorPropertyKind::UNIT},
    {"End Hide", EditorPropertyKind::UNIT},
    {"Gravity", EditorPropertyKind::ACCELERATION},
    {"Drag", EditorPropertyKind::FACTOR},
    {"Stretch", EditorPropertyKind::FINE},
    {"Flash", EditorPropertyKind::FACTOR},
    {"Flash Range", EditorPropertyKind::EXTENT},
    {"Flash Time", EditorPropertyKind::DURATION},
    {"Height", EditorPropertyKind::EXTENT},
    {"Columns", EditorPropertyKind::COUNT, 64.0f},
    {"Rows", EditorPropertyKind::COUNT, 64.0f},
    {"Frames", EditorPropertyKind::COUNT, 4096.0f},
    {"Frames/s", EditorPropertyKind::FACTOR},
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

/// Whether a field is on or off, which the panel draws as a checkbox.
[[nodiscard]] constexpr bool
editorPropertyFieldIsToggle(EditorPropertyField field) {
  return editorPropertyFieldKind(field) == EditorPropertyKind::TOGGLE;
}

/// Whether a field is a size multiplier, which the panel draws as a slider.
[[nodiscard]] constexpr bool
editorPropertyFieldIsScale(EditorPropertyField field) {
  return editorPropertyFieldKind(field) == EditorPropertyKind::SCALE;
}

/// Whether a field is an angle, which decides how it is written out and
/// whether it wraps.
[[nodiscard]] constexpr bool
editorPropertyFieldIsAngle(EditorPropertyField field) {
  return editorPropertyFieldKind(field) == EditorPropertyKind::ANGLE;
}

}  // namespace eng::editor

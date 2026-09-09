#pragma once

/// @file editor-property-field.h
/// @brief The properties the panel lists, one row each.
/// @par Threading Thread-safe (immutable value types).

#include <cstddef>
#include <cstdint>

namespace eng::editor {

/// One editable number on whatever the editor has selected.
///
/// The panel is a list of these rather than a hand-written form per kind of
/// selection: the rows, the hit testing, and the edits all run off this
/// enum, so a light's intensity is another entry here and another arm in
/// `editor-property-ops.cpp` rather than a second panel.
///
/// One enum covers placements and lights together, so a field two of them
/// share — a position — is one row definition and one edit path. The three
/// members of every triple are declared in component order, which is what
/// lets `editor-property-ops.cpp` take a component index by subtraction
/// rather than a case per axis.
/// @thread_safety Immutable value type.
enum class EditorPropertyField : uint8_t {
  /// World X, in tiles.
  POSITION_X,
  /// World Y, in tiles.
  POSITION_Y,
  /// Height above the ground plane, in tiles.
  POSITION_Z,
  /// Rotation about world X, in degrees.
  ROTATION_X,
  /// Rotation about world Y, in degrees.
  ROTATION_Y,
  /// Rotation about world Z, in degrees.
  ROTATION_Z,
  /// X of the direction a light arrives from.
  DIRECTION_X,
  /// Y of the direction a light arrives from.
  DIRECTION_Y,
  /// Z of the direction a light arrives from.
  DIRECTION_Z,
  /// Red channel of a light's tint.
  COLOR_R,
  /// Green channel of a light's tint.
  COLOR_G,
  /// Blue channel of a light's tint.
  COLOR_B,
  /// Brightness multiplier of a light.
  INTENSITY,
  /// How far a point light reaches, in tiles.
  RANGE,
};

/// Every field there is, in the enum's own order.
///
/// The traits table beside it is indexed by this order and asserts against
/// this length, so a field added to the enum and forgotten here fails the
/// build rather than reading another field's label.
inline constexpr EditorPropertyField EDITOR_ALL_PROPERTY_FIELDS[] = {
    EditorPropertyField::POSITION_X,  EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z,  EditorPropertyField::ROTATION_X,
    EditorPropertyField::ROTATION_Y,  EditorPropertyField::ROTATION_Z,
    EditorPropertyField::DIRECTION_X, EditorPropertyField::DIRECTION_Y,
    EditorPropertyField::DIRECTION_Z, EditorPropertyField::COLOR_R,
    EditorPropertyField::COLOR_G,     EditorPropertyField::COLOR_B,
    EditorPropertyField::INTENSITY,   EditorPropertyField::RANGE,
};

/// What the panel lists for a placed asset, in the order it lists them.
inline constexpr EditorPropertyField EDITOR_PLACEMENT_FIELDS[] = {
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z, EditorPropertyField::ROTATION_X,
    EditorPropertyField::ROTATION_Y, EditorPropertyField::ROTATION_Z,
};

/// What the panel lists for a directional light.
///
/// No position: a light with parallel rays shades a scene the same wherever
/// it stands, and a row that changes nothing but a marker is a row that
/// lies about what it does.
inline constexpr EditorPropertyField EDITOR_DIRECTIONAL_LIGHT_FIELDS[] = {
    EditorPropertyField::DIRECTION_X, EditorPropertyField::DIRECTION_Y,
    EditorPropertyField::DIRECTION_Z, EditorPropertyField::INTENSITY,
    EditorPropertyField::COLOR_R,     EditorPropertyField::COLOR_G,
    EditorPropertyField::COLOR_B,
};

/// What the panel lists for a point light: where it is and how far it
/// carries, in place of the direction it has none of.
inline constexpr EditorPropertyField EDITOR_POINT_LIGHT_FIELDS[] = {
    EditorPropertyField::POSITION_X, EditorPropertyField::POSITION_Y,
    EditorPropertyField::POSITION_Z, EditorPropertyField::RANGE,
    EditorPropertyField::INTENSITY,  EditorPropertyField::COLOR_R,
    EditorPropertyField::COLOR_G,    EditorPropertyField::COLOR_B,
};

/// How many rows a placement's properties fill.
inline constexpr size_t EDITOR_PLACEMENT_FIELD_COUNT =
    sizeof(EDITOR_PLACEMENT_FIELDS) / sizeof(EDITOR_PLACEMENT_FIELDS[0]);

}  // namespace eng::editor

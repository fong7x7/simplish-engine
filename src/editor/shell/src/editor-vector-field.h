#pragma once

/// @file editor-vector-field.h
/// @brief Reaching one component of a vector from the field that names it.
///
/// Private to the package: it exists so that the placement's properties and
/// the light's reach a position, a rotation, a direction and a colour the
/// same way, rather than each writing a case per axis.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>

namespace eng::editor {

/// Which of a triple's three components @p field names, given @p first, the
/// field that triple begins at.
///
/// Every triple in `EditorPropertyField` is declared in component order, so
/// the distance between the two is the component index. A field outside the
/// triple gives a nonsense index, which is why the callers ask whether a
/// field belongs to a triple before asking which component it is.
[[nodiscard]] inline size_t editorFieldAxis(EditorPropertyField field,
                                            EditorPropertyField first) {
  return static_cast<size_t>(static_cast<uint8_t>(field) -
                             static_cast<uint8_t>(first));
}

/// Whether @p field is one of the three starting at @p first.
[[nodiscard]] inline bool editorFieldInTriple(EditorPropertyField field,
                                              EditorPropertyField first) {
  return editorFieldAxis(field, first) < 3;
}

/// The component of @p vector that @p axis names, to read or to write. An
/// axis past the third reads and writes Z, which is the same answer the
/// callers' bounds check has already ruled out.
[[nodiscard]] inline float& editorVectorAxis(Vec3& vector, size_t axis) {
  return axis == 0 ? vector.x : (axis == 1 ? vector.y : vector.z);
}

/// The component of @p vector that @p axis names.
[[nodiscard]] inline float editorVectorValue(const Vec3& vector, size_t axis) {
  return axis == 0 ? vector.x : (axis == 1 ? vector.y : vector.z);
}

/// The component of a world position that @p axis names, to read or write.
/// A world point is its own type rather than a vector, so it needs its own
/// pair of these; the alternative is a cast between two layouts that are
/// only incidentally the same.
[[nodiscard]] inline float& editorVectorAxis(WorldPoint& point, size_t axis) {
  return axis == 0 ? point.x : (axis == 1 ? point.y : point.z);
}

/// The component of a world position that @p axis names.
[[nodiscard]] inline float editorVectorValue(const WorldPoint& point,
                                             size_t axis) {
  return axis == 0 ? point.x : (axis == 1 ? point.y : point.z);
}

}  // namespace eng::editor

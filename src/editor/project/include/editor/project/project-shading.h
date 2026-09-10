#pragma once

/// @file project-shading.h
/// @brief How a project's meshes are shaded: smooth, or cel-shaded.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// The look a project's meshes are drawn with.
///
/// A project setting, as the projection is, because it is the game's art
/// style rather than a viewing preference: a cel-shaded game's props are
/// modelled and textured for flat tones and hard lines, and the editor
/// should show them the way the game will. Unlike the projection it can be
/// switched freely — nothing authored depends on it, and the renderer
/// changes it between one frame and the next.
/// @thread_safety Immutable value type.
enum class ProjectShading : uint8_t {
  /// Smooth light and no outline: meshes as they have always drawn.
  SMOOTH,
  /// Light flattened into a few tones, and a dark line along every
  /// silhouette and crease — `MESH_STYLE_CEL`.
  CEL,
};

/// Every shading, in enum order.
inline constexpr ProjectShading PROJECT_SHADINGS[] = {
    ProjectShading::SMOOTH,
    ProjectShading::CEL,
};

/// The name this shading is written as in `project.json`.
[[nodiscard]] constexpr std::string_view
projectShadingName(ProjectShading shading) {
  return shading == ProjectShading::CEL ? "cel" : "smooth";
}

/// Read a shading back from a manifest.
///
/// Anything unrecognised — including the field being absent, which is every
/// project written before the setting existed — reads as smooth, which is
/// how those projects have always looked.
[[nodiscard]] constexpr ProjectShading
projectShadingFromName(std::string_view name) {
  return name == "cel" ? ProjectShading::CEL : ProjectShading::SMOOTH;
}

}  // namespace eng::editor

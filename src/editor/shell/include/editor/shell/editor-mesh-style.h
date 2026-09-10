#pragma once

/// @file editor-mesh-style.h
/// @brief The engine's mesh style for a project's shading setting.
/// @par Threading Thread-safe (pure function over value types).

#include <editor/project/project-shading.h>
#include <engine/render-mesh/mesh-style.h>

namespace eng::editor {

/// The style the viewport draws meshes with under @p shading.
///
/// The project stores a word rather than the numbers: a project picks a
/// look, and the engine's presets say what that look is. A game wanting a
/// thicker line or a fourth band builds its own `MeshStyle`; the editor
/// does not grow a panel of sliders nobody has asked for.
[[nodiscard]] constexpr MeshStyle editorMeshStyleFor(ProjectShading shading) {
  return shading == ProjectShading::CEL ? MESH_STYLE_CEL : MESH_STYLE_SMOOTH;
}

}  // namespace eng::editor

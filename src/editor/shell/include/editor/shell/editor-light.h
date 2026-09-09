#pragma once

/// @file editor-light.h
/// @brief One light source placed in the level.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>
#include <engine/render-mesh/mesh-light.h>
#include <string>

namespace eng::editor {

/// What shape a light throws.
///
/// The two the renderer's lighting model names ([Engine REQUIREMENTS §5.4]):
/// a directional key light standing in for the sun, and point lights for
/// everything local. Spot lights wait for the cone the shader does not read
/// yet, so offering one here would be a control that changes nothing.
/// @thread_safety Immutable value type.
enum class EditorLightKind : uint8_t {
  /// Parallel rays from infinitely far away. Shades the same wherever it
  /// stands, so its position is only where its marker sits.
  DIRECTIONAL,
  /// Rays from one world position, falling off to nothing at its range.
  POINT,
};

/// A light placed in the world.
///
/// One record for both kinds rather than a type per kind: they differ by
/// three numbers, the properties panel lists whichever the kind uses, and
/// the renderer walks one array. Lights are saved and loaded with the level
/// as placements are — see `editor-placement.h`.
/// @thread_safety Main-thread-only.
struct EditorLight {
  /// Stable identifier for this one light: `point_01`. Assigned when it is
  /// added and never reused, so a logic file can switch this light by name
  /// — see `editor-entity-id.h`.
  std::string id{};
  /// Which shape this light throws.
  EditorLightKind kind = EditorLightKind::DIRECTIONAL;
  /// Where the light stands, and where its marker is drawn.
  WorldPoint position{};
  /// Unit vector from a lit surface *towards* the light — the direction its
  /// rays arrive from, not the direction they travel. That is the vector
  /// shading needs, and holding it the other way round would mean negating
  /// it in two renderers and reading it negated in the panel. Defaults to
  /// the built-in key light's direction, so a light left alone lights the
  /// scene the way it was already lit.
  Vec3 direction = MESH_KEY_LIGHT_DIRECTION;
  /// Linear RGB tint, each component in [0, 1].
  Vec3 color{1.0f, 1.0f, 1.0f};
  /// Brightness multiplier. One is the built-in key light's strength, so a
  /// light left at its default lands the scene where it already was.
  float intensity = 1.0f;
  /// How far a point light reaches, in tiles. Nothing beyond this is lit by
  /// it, which is what keeps a scene's lights to the few that matter.
  /// Unused by a directional light, which never falls off.
  float range = 8.0f;
};

}  // namespace eng::editor

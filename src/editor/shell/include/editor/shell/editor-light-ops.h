#pragma once

/// @file editor-light-ops.h
/// @brief Make, read, write, and shade with the editor's light sources.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-light.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/iso-projection.h>
#include <engine/render-mesh/mesh-light.h>
#include <span>
#include <string_view>

namespace eng::editor {

/// How high above the ground a dropped light hangs, in tiles.
///
/// Over head height and above anything on the tile it was dropped on, so a
/// point light lights the props around it rather than sitting inside one.
inline constexpr float EDITOR_LIGHT_DROP_HEIGHT = 3.0f;

/// Half the width of the box a light is drawn and picked as.
///
/// A light has no geometry, so this is only a handle to click; it is small
/// enough to sit inside a tile and not be mistaken for a prop.
inline constexpr float EDITOR_LIGHT_MARKER_RADIUS = 0.25f;

/// What a light of this kind is called, in the browser and the panel.
[[nodiscard]] std::string_view editorLightKindName(EditorLightKind kind);

/// A new light of @p kind at @p position, with the defaults a dropped one
/// gets: the key light's direction and strength, white, and a range that
/// covers the tiles around it.
[[nodiscard]] EditorLight makeEditorLight(EditorLightKind kind,
                                          WorldPoint position);

/// The properties the panel lists for a light of @p kind, in row order.
[[nodiscard]] std::span<const EditorPropertyField>
editorLightFields(EditorLightKind kind);

/// Current value of one of a light's properties. A field the light does not
/// have — a placement's rotation — reads as zero.
[[nodiscard]] float editorLightValue(const EditorLight& light,
                                     EditorPropertyField field);

/// Write one of a light's properties, normalised as
/// `normalizeEditorPropertyValue` defines. A field the light does not have
/// is ignored.
void setEditorLightValue(EditorLight& light, EditorPropertyField field,
                         float value);

/// The light as the renderers read it, in world units.
[[nodiscard]] MeshLight makeMeshLight(const EditorLight& light);

}  // namespace eng::editor

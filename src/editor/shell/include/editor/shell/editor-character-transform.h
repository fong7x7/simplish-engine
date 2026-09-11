#pragma once

/// @file editor-character-transform.h
/// @brief Object-to-world transform for a model drawn as a player.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-asset.h>
#include <engine/math/mat4.h>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>

namespace eng::editor {

/// Stand @p asset on @p feet as a player, facing along @p aim.
///
/// Scaled by height rather than by footprint as a prop is: a character is
/// as tall as the player is to collision (`game::PLAYER_HEIGHT_TILES`),
/// whatever unit its author worked in, and keeps its own proportions — so
/// a model with its arms out is not shrunk to fit a tile. It is centred on
/// @p feet horizontally with its lowest point on them.
///
/// A model is taken to face the way glTF's convention has it, along its
/// own +Z, which the Z-up conversion turns to world -Y; it is turned from
/// there to face @p aim. An @p aim of zero length faces +X, the direction
/// a player spawns aiming in.
///
/// An asset whose bounds were never measured keeps its own units, for the
/// reason `makePlacementTransform` gives.
[[nodiscard]] Mat4 makeEditorCharacterTransform(const EditorAsset& asset,
                                                Vec3 feet, Vec2 aim);

}  // namespace eng::editor

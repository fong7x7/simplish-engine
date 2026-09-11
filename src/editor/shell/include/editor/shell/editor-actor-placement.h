#pragma once

/// @file editor-actor-placement.h
/// @brief Props that are actors: which ones, what the game is told of them,
/// and where the editor draws them while the game moves them.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-placement.h>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <game/actors/actor-spawn.h>
#include <string>
#include <vector>

namespace eng::editor {

/// The turn from a prop's Z rotation to the way it faces, in degrees.
///
/// A model is taken to face the way glTF's convention has it — along its own
/// +Z, which the Z-up conversion turns to world -Y — as a player's model is
/// (`makeEditorCharacterTransform`). At zero rotation a prop therefore faces
/// -Y, a quarter turn clockwise of the +X a facing angle counts from.
inline constexpr float EDITOR_MODEL_FRONT_DEGREES = -90.0F;

/// The smallest and largest radius an actor is given from its model's
/// footprint, in tiles: something to collide with, and no wider than a tile
/// either side.
inline constexpr float EDITOR_ACTOR_MIN_RADIUS = 0.15F;
/// The largest radius an actor is given, in tiles.
inline constexpr float EDITOR_ACTOR_MAX_RADIUS = 1.0F;

/// Whether @p placement is an actor: a prop with a behavior.
[[nodiscard]] bool isEditorActor(const EditorPlacement& placement);

/// The index in @p document's placements of every actor, in document order —
/// which is the order a playtest spawns them in, so the Nth of these is the
/// game's Nth actor.
[[nodiscard]] std::vector<size_t>
editorActorPlacements(const EditorDocument& document);

/// The id of every actor, in the order `editorActorPlacements` lists them.
[[nodiscard]] std::vector<std::string>
editorActorIds(const EditorDocument& document);

/// The way @p placement faces, in degrees counterclockwise from world +X:
/// its Z rotation plus `EDITOR_MODEL_FRONT_DEGREES`. X and Y rotation tip a
/// model over and do not turn it.
[[nodiscard]] float editorActorYawDegrees(const EditorPlacement& placement);

/// What the game is told of the actor @p placement is, measured against its
/// @p asset: feet at the centre of its tile at its height, facing its yaw,
/// running its behavior on its side, as wide as half the narrower side of
/// its unturned footprint and as tall as its model.
[[nodiscard]] game::ActorSpawn
editorActorSpawn(const EditorPlacement& placement, const EditorAsset& asset);

/// @p placement as drawn where the game has its actor: feet at @p feet,
/// turned to face @p facing — which keeps its own heading when it is zero —
/// with its X and Y rotation, scale and clip untouched.
[[nodiscard]] EditorPlacement editorActorPose(const EditorPlacement& placement,
                                              Vec3 feet, Vec2 facing);

/// The unit direction @p placement faces, on the floor.
[[nodiscard]] Vec2 editorActorFacing(const EditorPlacement& placement);

}  // namespace eng::editor
